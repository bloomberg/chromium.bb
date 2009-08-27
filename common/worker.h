/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Worker thread class
// Used by some of the multi-threaded demos.

#ifndef NATIVE_CLIENT_TESTS_COMMON_WORKER_H_
#define NATIVE_CLIENT_TESTS_COMMON_WORKER_H_

// typdef helper for pthread_create
typedef void* (*Entry)(void *data);

// WorkerThreadManager is a class to manage N worker threads
// It includes a mutex protected counter so each thread
// can derive work from a uid.  It also includes two semaphores
// used to synchronize the workers and the main thread.

class WorkerThreadManager {
#if defined(HAVE_THREADS)
 public:
  void SetCounter(int v);
  int DecCounter();
  void PostWork() { sem_post(&work_); }
  void PostWorkAll()
    { for (int i = 0; i < num_threads_; ++i) sem_post(&work_); }
  void WaitWork() { sem_wait(&work_); }
  void PostDone() { sem_post(&done_); }
  void WaitDone() { sem_wait(&done_); }
  bool CreateThreadPool(int num, void *(*entry)(void *data), void *data);
  void JoinAll();
  WorkerThreadManager();
  ~WorkerThreadManager() { ; }
 private:
  // pthread_attr_t attr_;
  pthread_t *threads_;
  pthread_mutex_t mutex_;
  int counter_;
  int num_threads_;
  sem_t work_;
  sem_t done_;
#else
 public:
  void SetCounter(int v) { ; }
  int DecCounter() { return 0; }
  void PostWork() { ; }
  void PostWorkAll() { ; }
  void WaitWork() { ; }
  void PostDone() { ; }
  void WaitDone() { ; }
  bool CreateThreadPool(int num, void *(*entry)(void *data), void *data) {
    return false;
  }
  void JoinAll() { ; }
  WorkerThreadManager() { ; }
  ~WorkerThreadManager() { ; }
#endif  // HAVE_THREADS
};


#if defined(HAVE_THREADS)

// Sets the mutex protected counter to specified value.
inline void WorkerThreadManager::SetCounter(int v) {
  pthread_mutex_lock(&mutex_);
  { counter_ = v; }
  pthread_mutex_unlock(&mutex_);
}


// Decrements and get the value of the mutex protected counter
inline int WorkerThreadManager::DecCounter() {
  int v;
  pthread_mutex_lock(&mutex_);
  { v = --counter_; }
  pthread_mutex_unlock(&mutex_);
  return v;
}


// Initializes mutex & semaphores used by worker manager.
// The thread poll will be created later.
inline WorkerThreadManager::WorkerThreadManager()
: threads_(NULL), counter_(0), num_threads_(0) {
  int status;
  status = pthread_mutex_init(&mutex_, NULL);
  if (0 != status) {
    fprintf(stderr, "Failed to initialize mutex!\n");
    exit(-1);
  }
  status = sem_init(&work_, 0, 0);
  if (-1 == status) {
    fprintf(stderr, "Failed to initialize semaphore!\n");
    exit(-1);
  }
  status = sem_init(&done_, 0, 0);
  if (-1 == status) {
    fprintf(stderr, "Failed to initialize semaphore!\n");
    exit(-1);
  }
}


// Creates a pool of detached worker threads.
inline bool WorkerThreadManager::CreateThreadPool(int num,
                                                  Entry entry, void *data) {
  if (NULL != threads_) {
    // shouldn't happen, but we should at least tell user
    fprintf(stderr, "WARNING: A thread pool has already been created!\n");
    return true;
  }
  // create worker threads
  threads_ = new pthread_t[num];
  num_threads_ = num;
  printf("Starting up %d worker threads.\n", num);
  for (int i = 0; i < num; ++i) {
    int status = pthread_create(&threads_[i], NULL, entry, data);
    if (0 != status) {
      fprintf(stderr, "Failed to allocate thread %d!\n", i);
      return false;
    }
  }
  return true;
}


// join up all the threads in the pool
inline void WorkerThreadManager::JoinAll() {
  void *retval;
  for (int i = 0; i < num_threads_; ++i) {
    pthread_join(threads_[i], &retval);
  }
}

#endif  // HAVE_THREADS
#endif  // NATIVE_CLIENT_TESTS_COMMON_WORKER_H_
