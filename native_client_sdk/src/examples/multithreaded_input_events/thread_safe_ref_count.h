// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THREAD_SAFE_REF_COUNT_H
#define THREAD_SAFE_REF_COUNT_H

#include <pthread.h>
#include <cassert>

const int kPthreadMutexSuccess = 0;

namespace event_queue {
// Some synchronization helper classes.

class ScopedLock {
 public:
  explicit ScopedLock(pthread_mutex_t* mutex)
      : mutex_(mutex) {
    if (pthread_mutex_lock(mutex_) != kPthreadMutexSuccess) {
      mutex_ = NULL;
    }
  }
  ~ScopedLock() {
    if (mutex_ != NULL) {
      pthread_mutex_unlock(mutex_);
    }
  }
 private:
  ScopedLock& operator=(const ScopedLock&);  // Not implemented, do not use.
  ScopedLock(const ScopedLock&);  // Not implemented, do not use.
  pthread_mutex_t* mutex_;  // Weak reference, passed in to constructor.
};

class ThreadSafeRefCount {
 public:
  ThreadSafeRefCount()
      : ref_(0) {
    Init();
  }

  void Init() {
    pthread_mutex_init(&mutex_, NULL);
  }

  int32_t AddRef() {
    ScopedLock s(&mutex_);
    return ++ref_;
  }

  int32_t Release() {
    ScopedLock s(&mutex_);
    return --ref_;
  }

 private:
  int32_t ref_;
  pthread_mutex_t mutex_;
};

}  // namespace
#endif  // THREAD_SAFE_REF_COUNT_H

