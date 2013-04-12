// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_ATOMICOPS_H_
#define GPU_COMMAND_BUFFER_CLIENT_ATOMICOPS_H_

#include "base/memory/scoped_ptr.h"
#include "../../gpu_export.h"
#include "../common/types.h"

namespace gpu {

GPU_EXPORT void MemoryBarrier();

class LockImpl;
class GPU_EXPORT Lock {
 public:
  Lock();
  ~Lock();
  void Acquire();
  void Release();
  bool Try();
  void AssertAcquired() const;

 private:
  scoped_ptr<LockImpl> lock_;

  DISALLOW_COPY_AND_ASSIGN(Lock);
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
class GPU_EXPORT AutoLock {
 public:
  explicit AutoLock(Lock& lock) : lock_(lock) {
    lock_.Acquire();
  }

  ~AutoLock() {
    lock_.AssertAcquired();
    lock_.Release();
  }

 private:
  Lock& lock_;
  DISALLOW_COPY_AND_ASSIGN(AutoLock);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_ATOMICOPS_H_
