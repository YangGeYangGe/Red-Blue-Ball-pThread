#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RED "\x1B[31m"
#define GRED "\x1b[41m"
#define BLUE "\x1b[34m"
#define GBLUE "\x1b[44m"
#define GWHITE "\x1b[47m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define BLACK "\x1b[30m"

typedef struct{
    pthread_mutex_t count_lock;		/* mutex semaphore for the barrier */
    pthread_cond_t ok_to_proceed;	/* condition variable for leaving */
    int	count;		/* count of the number who have arrived */
} mylib_barrier_t;

typedef struct{
    int idx;
    int **grid;

    int tile_amount;
    int starting_tile_row;
}thread_args;

//simple barrier
void mylib_barrier_init(mylib_barrier_t *b);
void mylib_barrier(mylib_barrier_t *b, int num_threads) ;
void mylib_barrier_destroy(mylib_barrier_t *b) ;

//structure: red movement->barrier->blue movement
void *Movement(void *threadid);

//similar to assignment 1
void red_movement(int r, int c, int **gridr);
void blue_movement(int r, int c, int **gridb);
void swap(int *a, int *b);
void randomize(int arr[], int n);
void board_init(int cell_size, int **grid);
void print_cell(int cell_size, int **grid) ;
// int finished(int n, int **grid, int tile_size, int threshold);
int finished(int tile_row_amount,int starting_tile_row, int n, int **grid, int tile_size, int threshold);
void print_gird(int n, int **tmpgrid, int tile_size, int threshold);

mylib_barrier_t	barrier;
int NUM_THREADS;
int process_load;
int n;
int t;
int c;
int max_iters;
int each_tile_size;
int current_iter;
int termination_chekcing;
int main(int argc, char *argv[]) {
    if (argc != 6) {
		printf("Please enter correct input! arguments: [grid_size] [tile] [threshold] [max_iters] [num_threads]\n");
		return 0;
	}
    n = atoi(argv[1]);
	t = atoi(argv[2]);
	c = atoi(argv[3]);
    max_iters = atoi(argv[4]);
    NUM_THREADS = atoi(argv[5]);

    if(n %  NUM_THREADS != 0){
        printf("%d, %d\n",n, NUM_THREADS);
        printf("grid_size/NUM_THREADS not divisible !\n" );
        return 0;
    }
    int i,j;

    int *tile_row_each_processor = (int *)calloc(n, sizeof(int *));
    if (NUM_THREADS >= t) {
        for(i = 0; i < t; i++){
            tile_row_each_processor[i] = 1;
        }
    } else {
        if(t % NUM_THREADS == 0){
            for(i = 0; i < NUM_THREADS; i++){
                tile_row_each_processor[i] = t / NUM_THREADS;
            }
        } else {
            for(i = 0; i < NUM_THREADS; i++){
                if((t % NUM_THREADS) > i){
                    tile_row_each_processor[i] = t / NUM_THREADS + 1;
                } else{
                    tile_row_each_processor[i] = t / NUM_THREADS;
                }
            }
        }
    }

    for(i = 0; i < NUM_THREADS; i++){
        printf("%d ", tile_row_each_processor[i] );
    }
    printf("\n");
    process_load = n/NUM_THREADS;
    each_tile_size = n / t;
    //set grid
	int **grid = (int **)malloc(n * sizeof(int *));
	for (i=0; i<n; i++){
		grid[i] = (int *)malloc(n * sizeof(int));
	}
    //make a grid copy for self checking part
    int **self_checking = (int **)malloc(n * sizeof(int *));
	for (i=0; i<n; i++){
		self_checking[i] = (int *)malloc(n * sizeof(int));
	}

    //initialize grid
    board_init(n, grid);
    //make copy
    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            self_checking[i][j] = grid[i][j];
        }
    }
    //check initial status
    if(finished(t,0,n, grid, each_tile_size, c) == 1){
        printf("initial grid does not need to move\n");
        return 0;
    }
    pthread_t threads[NUM_THREADS];
    int rc;
    //thread_args: thread index(id), the complete grid
    thread_args tids[NUM_THREADS];
    //make joinable
    pthread_attr_t attr;
    mylib_barrier_init(&barrier);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    //set arguments for all threads
    int current_tile = 0;
	for(i=0;i<NUM_THREADS;i++){
        tids[i].idx = i;
        tids[i].grid = grid;
        tids[i].tile_amount = tile_row_each_processor[i];
        if( i == 0 ){
            tids[i].starting_tile_row = 0;
        } else {
            tids[i].starting_tile_row = tids[i - 1].starting_tile_row + tids[i-1].tile_amount;
        }

	}
	current_iter = 0;
    termination_chekcing = 0;
    for(i=0;i<NUM_THREADS;i++){
        rc = pthread_create(&threads[i], &attr, Movement, (void *) &tids[i]);
        if (rc){
            printf("ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }
    for (i = 0; i < NUM_THREADS; i++){
        pthread_join(threads[i], NULL);
    }
    //finished movements
    printf("finished movements, result:\n" );
    print_gird(n, grid, each_tile_size, c);
    printf("current_iter %d\n", current_iter);
    //sefl checking, use the grid copy
    int check_flag = 0;
    current_iter = 0;
	while(finished(t,0,n, self_checking, each_tile_size, c) == 0 && current_iter < max_iters){
        red_movement(n,n,self_checking);
        blue_movement(n,n,self_checking);
        current_iter++;
    }
    for(i = 0; i < n; i++){
        for(j = 0; j < n; j++){
            if(self_checking[i][j] != grid[i][j]){
                check_flag = 1;
                break;
            }
        }
    }
    if(check_flag == 0){
        printf("All good\n" );
    }else{
        printf("Something wrong\n");
    }
    pthread_exit(NULL);
    return 0;
}
//thread function
void *Movement(void *threadid) {
    thread_args *tid;
    tid = (thread_args *) threadid;
    int idx = tid->idx;
    int **grid = tid->grid;
    int tile_amount = tid->tile_amount;
    int i, j;
    while(termination_chekcing != 1 && current_iter < max_iters){
        for (i = idx * process_load ; i < idx * process_load + process_load ; i++) {
            if (grid[i][0] == 1 && grid[i][1] == 0) {
                grid[i][0] = 4;
                grid[i][1] = 3;
            }
            for (j = 1; j < n; j++) {
                if (grid[i][j] == 1 && (grid[i][(j + 1) % n] == 0)) {
                    grid[i][j] = 0;
                    grid[i][(j + 1) % n] = 3;
                }
                else if (grid[i][j] == 3) {
                    grid[i][j] = 1;
                }
            }
            if (grid[i][0] == 3) {
                grid[i][0] = 1;
            }
            else if (grid[i][0] == 4) {
                grid[i][0] = 0;
            }
        }
        mylib_barrier(&barrier, NUM_THREADS);
        for (j = idx * process_load; j < idx * process_load + process_load; j++) {
            if (grid[0][j] == 2 && grid[1][j] == 0) {
                grid[0][j] = 4;
                grid[1][j] = 3;
            }
            for (i = 0; i < n; i++) {
                if (grid[i][j] == 2 && (grid[(i + 1) % n][j] == 0)) {
                    grid[i][j] = 0;
                    grid[(i + 1) % n][j] = 3;
                }
                else if (grid[i][j] == 3) {
                    grid[i][j] = 2;
                }
            }
            if (grid[0][j] == 3) {
                grid[0][j] = 2;
            }
            else if (grid[0][j] == 4) {
                grid[0][j] = 0;
            }
        }
        mylib_barrier(&barrier, NUM_THREADS);
        if(tile_amount != 0){
            if(finished(tile_amount, tid-> starting_tile_row ,n, grid, each_tile_size, c) == 1){
                termination_chekcing = 1;
            }
        }
        if(idx == 0){
            current_iter++;
        }
        mylib_barrier(&barrier, NUM_THREADS);
    }
    pthread_exit(NULL);
}

//barrier implementation
void mylib_barrier_init(mylib_barrier_t *b) {
    b -> count = 0;
    pthread_mutex_init(&(b -> count_lock), NULL);
    pthread_cond_init(&(b -> ok_to_proceed), NULL);
}

void mylib_barrier(mylib_barrier_t *b, int num_threads) {
    pthread_mutex_lock(&(b -> count_lock));
    b -> count++;
    if (b -> count == num_threads){
        b -> count = 0;
        pthread_cond_broadcast( &(b -> ok_to_proceed) );
    }
    else{
        pthread_cond_wait( &(b -> ok_to_proceed) , &(b -> count_lock));
    }
    pthread_mutex_unlock(&(b -> count_lock));
}


void mylib_barrier_destroy(mylib_barrier_t *b) {
    pthread_mutex_destroy( &(b -> count_lock) );
    pthread_cond_destroy(&(b -> ok_to_proceed));
}

//functions bellow are similar to assignemtn 1
void swap(int *a, int *b) {
	int temp = *a;
	*a = *b;
	*b = temp;
}
void randomize(int arr[], int n){
	srand(time(NULL));
	int i = 0;
	for (i = n - 1; i > 0; i--)
	{
		int j = rand() % (i + 1);
		swap(&arr[i], &arr[j]);
	}
}

void board_init(int cell_size, int **grid) {
    int arraysize = cell_size*cell_size;
	int tmp[arraysize];
	int i, j;
	for (i = 0; i < arraysize; i++) {
		if (i < arraysize / 3) {
			tmp[i] = 1;
		}
		else if (i < 2 * arraysize / 3) {
			tmp[i] = 2;
		}
		else {
			tmp[i] = 0;
		}
	}
randomize(tmp, arraysize);
	int count = 0;
	for (i = 0; i < cell_size; i++) {
		for (j = 0; j < cell_size; j++) {
			grid[i][j] = tmp[count];
			count++;
		}
	}
}
void red_movement(int r, int c, int **gridr) {
	int i, j;
	int n = c;
	for (i = 0; i < r; i++) {
		if (gridr[i][0] == 1 && gridr[i][1] == 0) {
			gridr[i][0] = 4;
			gridr[i][1] = 3;
		}
		for (j = 1; j < c; j++) {
			if (gridr[i][j] == 1 && (gridr[i][(j + 1) % n] == 0)) {
				gridr[i][j] = 0;
				gridr[i][(j + 1) % n] = 3;
			}
			else if (gridr[i][j] == 3) {
				gridr[i][j] = 1;
			}
		}
		if (gridr[i][0] == 3) {
			gridr[i][0] = 1;
		}
		else if (gridr[i][0] == 4) {
			gridr[i][0] = 0;
		}
	}
}
void blue_movement(int r, int c, int **gridb) {
	int i, j;
	int n = c;
    for (j = 0; j < n; j++) {
        if (gridb[0][j] == 2 && gridb[1][j] == 0) {
            gridb[0][j] = 4;
            gridb[1][j] = 3;
        }
        for (i = 1; i < n; i++) {
            if (gridb[i][j] == 2 && (gridb[(i + 1) % n][j] == 0)) {
                gridb[i][j] = 0;
                gridb[(i + 1) % n][j] = 3;
            }
            else if (gridb[i][j] == 3) {
                gridb[i][j] = 2;
            }
        }
        if (gridb[0][j] == 3) {
            gridb[0][j] = 2;
        }
        else if (gridb[0][j] == 4) {
            gridb[0][j] = 0;
        }
    }
}

int finished(int tile_row_amount,int starting_tile_row,int n, int **grid, int tile_size, int threshold) {
	//change grid unit from cell to tile, then grid can be seen as (cell_size / tile_size) by (rs / tile_size).
	// int t = n / tile_size;
	int red_count = 0;
	int blue_count = 0;
	int a, b;
	int if_finished_result = 0;
	for (a = starting_tile_row; a < starting_tile_row+tile_row_amount; a++) {
		for (b = 0; b < t; b++) {
			//actual row number
			int row = a*tile_size;
			int col = b*tile_size;
			red_count = 0;
			blue_count = 0;
			//calculate red, blue number in a tile
			int a1, b1;
			for (a1 = row; a1 < row + tile_size; a1++) {
				for (b1 = col; b1 < col + tile_size; b1++) {
					if (grid[a1][b1] == 1) {
						red_count++;
					}
					else if (grid[a1][b1] == 2) {
						blue_count++;
					}
				}
			}
			int c_a = 0;
			int c_b = 0;
			if (((double)red_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if_finished_result = 1;
			}
			if (((double)blue_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
				if_finished_result = 1;
			}
		}
	}
	return if_finished_result;
}
void print_gird(int n, int **tmpgrid, int tile_size, int threshold) {
	int grid[n][n];
	int a, b;
	for (a = 0; a < n; a++) {
		for (b = 0; b < n; b++) {
			grid[a][b] = tmpgrid[a][b];
		}
	}
	int t = n / tile_size;
	int red_count = 0;
	int blue_count = 0;
	int if_finished_result;
	for (a = 0; a < t; a++) {
		for (b = 0; b < t; b++) {
			int row = a*tile_size;
			int col = b*tile_size;
			red_count = 0;
			blue_count = 0;
			int a1, b1;
			if_finished_result = 0;
			for (a1 = row; a1 < row + tile_size; a1++) {
				for (b1 = col; b1 < col + tile_size; b1++) {
					if (grid[a1][b1] == 1) {
						red_count++;
					}
					else if (grid[a1][b1] == 2) {
						blue_count++;
					}
				}
			}
			int c_a = 0;
			int c_b = 0;
			if (((double)red_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
                printf("%f red finished\n",(double)red_count * 100 / (tile_size*tile_size));
				if_finished_result = 1;
			}
			if (((double)blue_count * 100 / (tile_size*tile_size)) >  (double)threshold) {
                printf("%f blue finished\n", (double)blue_count * 100 / (tile_size*tile_size));
				if_finished_result = 1;
			}
			for (c_a = row; c_a < row + tile_size; c_a++) {
				for (c_b = col; c_b < col + tile_size; c_b++) {
					if (if_finished_result == 1) {
						if (grid[c_a][c_b] == 1) {
							grid[c_a][c_b] = 3;

						}
						else if (grid[c_a][c_b] == 2) {
							//printf(GBLUE"1 "ANSI_COLOR_RESET);
							grid[c_a][c_b] = 4;
						}
						else {
							grid[c_a][c_b] = 5;
						}
					}
				}
			}
		}
	}
	for (a = 0; a < n; a++) {
		for (b = 0; b < n; b++) {

			if (grid[a][b] == 1) {
				printf(RED"1 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 2) {
				printf(BLUE"2 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 3) {
				printf(GRED"1 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 4) {
				printf(GBLUE"2 "ANSI_COLOR_RESET);
			}
			else if (grid[a][b] == 0) {
				printf("0 ");
			}else if(grid[a][b] == 5) {
				printf(BLACK GWHITE"0 "ANSI_COLOR_RESET);
			}
			//printf("%d ", grid[a][b]);
		}
		printf("\n");
	}
}
