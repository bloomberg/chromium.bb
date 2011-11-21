// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SCOPED_MUTEX_LOCK_H_
#define SCOPED_MUTEX_LOCK_H_

#include "c_salt/threading/pthread_ext.h"

namespace threading {
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
}  // namespace threading

#endif  // SCOPED_MUTEX_LOCK_H_

