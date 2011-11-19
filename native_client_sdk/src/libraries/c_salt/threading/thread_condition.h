// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THREAD_CONDITION_H_
#define THREAD_CONDITION_H_

#include "c_salt/threading/pthread_ext.h"

struct timespec;

namespace c_salt {
namespace threading {
// A wrapper class for condition signaling.  Contains a mutex and condition
// pair.
class ThreadCondition {
 public:
  // Initialize the mutex and the condition.
  ThreadCondition() {
    pthread_mutex_init(&cond_mutex_, NULL);
    pthread_cond_init(&condition_, NULL);
  }

  virtual ~ThreadCondition() {
    pthread_cond_destroy(&condition_);
    pthread_mutex_destroy(&cond_mutex_);
  }

  // Lock the mutex, do this before signalling the condition.
  void Lock() {
    pthread_mutex_lock(&cond_mutex_);
  }

  // Unlock the mutex, do this after raising a signal or after returning from
  // Wait().
  void Unlock() {
    pthread_mutex_unlock(&cond_mutex_);
  }

  // Signal the condition.  This will cause Wait() to return.
  void Signal() {
    pthread_cond_broadcast(&condition_);
  }

  // Wait for a Signal().  Note that this can spuriously return, so you should
  // have a guard bool to see if the condtion is really true.  E.g., in the
  // calling thread:
  //   cond_lock->Lock();
  //   cond_true = true;
  //   cond_lock->Signal();
  //   cond_lock->Unlock();
  // In the worker thread:
  //   cond_lock->Lock();
  //   while (!cond_true) {
  //     cond_lock->Wait();
  //   }
  //   cond_lock->Unlock();
  void Wait() {
    pthread_cond_wait(&condition_, &cond_mutex_);
  }

  // Same as Wait, but wait at most until abs_time. Returns false if the system
  // time exceeds abs_time before the condition is signaled.
  bool TimedWait(struct timespec *abs_time) {
    return (pthread_cond_timedwait(&condition_, &cond_mutex_, abs_time) == 0);
  }

 private:
  pthread_mutex_t cond_mutex_;
  pthread_cond_t condition_;
};
}  // namespace threading
}  // namespace c_salt

#endif  // THREAD_CONDITION_H_

