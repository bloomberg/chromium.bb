// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../client/atomicops.h"
#include "../common/logging.h"

#if !defined(__native_client__)
#include "base/atomicops.h"
#include "base/synchronization/lock.h"
#else
#include <pthread.h>
#endif

namespace gpu {

void MemoryBarrier() {
#if defined(__native_client__)
  __sync_synchronize();
#else
  base::subtle::MemoryBarrier();
#endif
}

#if defined(__native_client__)

class LockImpl {
 public:
  LockImpl()
       : acquired_(false) {
    pthread_mutex_init(&mutex_, NULL);
  }

  ~LockImpl() {
    pthread_mutex_destroy(&mutex_);
  }

  void Acquire() {
    pthread_mutex_lock(&mutex_);
    acquired_ = true;
  }

  void Release() {
    GPU_DCHECK(acquired_);
    acquired_ = false;
    pthread_mutex_unlock(&mutex_);
  }

  bool Try() {
    bool acquired = pthread_mutex_trylock(&mutex_) == 0;
    if (acquired) {
      acquired_ = true;
    }
    return acquired;
  }

  void AssertAcquired() const {
    GPU_DCHECK(acquired_);
  }

 private:
  bool acquired_;
  pthread_mutex_t mutex_;

  DISALLOW_COPY_AND_ASSIGN(LockImpl);
};

#else  // !__native_client__

class LockImpl : public base::Lock {
};

#endif  // !__native_client__

Lock::Lock()
    : lock_(new LockImpl()) {
}

Lock::~Lock() {
}

void Lock::Acquire() {
  lock_->Acquire();
}

void Lock::Release() {
  lock_->Release();
}

bool Lock::Try() {
  return lock_->Try();
}

void Lock::AssertAcquired() const {
  return lock_->AssertAcquired();
}

}  // namespace gpu

