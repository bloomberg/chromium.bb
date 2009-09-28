// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_UTIL_PTHREAD_HELPERS_H_
#define CHROME_BROWSER_SYNC_UTIL_PTHREAD_HELPERS_H_

#include <pthread.h>
#include "base/logging.h"
#include "build/build_config.h"

#ifdef OS_WIN
typedef void* thread_handle;
#else
typedef pthread_t thread_handle;
#endif

// Creates a pthread, detaches from it, and returns a Win32 HANDLE for it that
// the caller must CloseHandle().
thread_handle CreatePThread(void* (*start)(void* payload), void* parameter);

template <typename LockType>
class PThreadScopedLock {
 public:
  explicit inline PThreadScopedLock(LockType* lock) : lock_(lock) {
    if (lock_)
      lock->Lock();
  }
  inline ~PThreadScopedLock() {
    Unlock();
  }
  inline void Unlock() {
    if (lock_) {
      lock_->Unlock();
      lock_ = NULL;
    }
  }
  LockType* lock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PThreadScopedLock);
};

class PThreadNoLock {
 public:
  inline void Lock() { }
  inline void Unlock() { }
};

// On win32, the pthread mutex implementation is about as efficient a critical
// section. It uses atomic operations and only needs kernel calls on
// contention.
class PThreadMutex {
 public:
  inline PThreadMutex() {
    pthread_mutexattr_t* attributes = NULL;
#ifndef NDEBUG
    private_attributes_in_use_ = true;
    pthread_mutexattr_init(&mutex_attributes_);
    pthread_mutexattr_settype(&mutex_attributes_, PTHREAD_MUTEX_ERRORCHECK);
    attributes = &mutex_attributes_;
#endif
    int result = pthread_mutex_init(&mutex_, attributes);
    DCHECK_EQ(0, result);
  }
  inline explicit PThreadMutex(const pthread_mutexattr_t* attr) {
#ifndef NDEBUG
    private_attributes_in_use_ = false;
#endif
    int result = pthread_mutex_init(&mutex_, attr);
    DCHECK_EQ(0, result);
  }
  inline ~PThreadMutex() {
    int result = pthread_mutex_destroy(&mutex_);
    DCHECK_EQ(0, result);
#ifndef NDEBUG
    if (private_attributes_in_use_) {
      pthread_mutexattr_destroy(&mutex_attributes_);
    }
#endif
  }
  inline void Lock() {
    int result = pthread_mutex_lock(&mutex_);
    DCHECK_EQ(0, result);
  }
  inline void Unlock() {
    int result = pthread_mutex_unlock(&mutex_);
    DCHECK_EQ(0, result);
  }
  pthread_mutex_t mutex_;

#ifndef NDEBUG
  pthread_mutexattr_t mutex_attributes_;
  bool private_attributes_in_use_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(PThreadMutex);
};

class PThreadCondVar {
 public:
  inline PThreadCondVar() {
    int result = pthread_cond_init(&condvar_, 0);
    DCHECK_EQ(0, result);
  }
  ~PThreadCondVar() {
    int result = pthread_cond_destroy(&condvar_);
    DCHECK_EQ(0, result);
  }
  inline void Signal() {
    int result = pthread_cond_signal(&condvar_);
    DCHECK_EQ(0, result);
  }
  inline void Broadcast() {
    int result = pthread_cond_broadcast(&condvar_);
    DCHECK_EQ(0, result);
  }
  pthread_cond_t condvar_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PThreadCondVar);
};

// Returns the absolute time ms milliseconds from now. Useful for passing
// result to pthread_cond_timedwait().
struct timespec GetPThreadAbsoluteTime(uint32 ms_from_now);

// Assign a descriptive label to the current thread. This is useful to see
// in a GUI debugger, but may not be implementable on all platforms.
void NameCurrentThreadForDebugging(const char* name);

#endif  // CHROME_BROWSER_SYNC_UTIL_PTHREAD_HELPERS_H_
