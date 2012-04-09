/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_UTILS_AUTO_LOCK_H_
#define LIBRARIES_UTILS_AUTO_LOCK_H_

#include <pthread.h>

class AutoLock {
 public:
  explicit AutoLock(pthread_mutex_t* lock) {
    lock_ = lock;
    pthread_mutex_lock(lock_);
  }
  ~AutoLock() {
    if (lock_) pthread_mutex_unlock(lock_);
  }

 private:
  pthread_mutex_t* lock_;
};

#endif  // LIBRARIES_UTILS_AUTO_LOCK_H_
