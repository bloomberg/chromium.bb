// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GTEST_PPAPI_SCOPED_MUTEX_LOCK_H_
#define GTEST_PPAPI_SCOPED_MUTEX_LOCK_H_

#include "gtest_ppapi/pthread_ext.h"

// A small helper RAII class that implements a scoped pthread_mutex lock.
class ScopedMutexLock {
 public:
  explicit ScopedMutexLock(pthread_mutex_t* mutex) : mutex_(mutex) {
    if (pthread_mutex_lock(mutex_) != PTHREAD_MUTEX_SUCCESS) {
      mutex_ = NULL;
    }
  }
  ~ScopedMutexLock() {
    if (mutex_)
      pthread_mutex_unlock(mutex_);
  }
  bool is_valid() const {
    return mutex_ != NULL;
  }
 private:
  pthread_mutex_t* mutex_;  // Weak reference.
};

#endif  // GTEST_PPAPI_SCOPED_MUTEX_LOCK_H_
