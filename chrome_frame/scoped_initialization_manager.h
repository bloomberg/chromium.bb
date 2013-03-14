// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_SCOPED_INITIALIZATION_MANAGER_H_
#define CHROME_FRAME_SCOPED_INITIALIZATION_MANAGER_H_

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"

namespace chrome_frame {

// A class intended to be instantiated on the stack in a dyanmically loaded
// shared object to initialize and shutdown the object's dependencies. |Traits|
// must be a type with two public static void(void) functions named Initialize
// and Shutdown. Traits::Initialize will be invoked when the first instance of
// this class is created and Traits::Shutdown will be invoked when the last one
// is destroyed.
template<class Traits>
class ScopedInitializationManager {
 public:
  ScopedInitializationManager() { AddRef(); }
  ~ScopedInitializationManager() { Release(); }

 private:
  static void AddRef() {
    base::AutoLock auto_lock(lock_.Get());
    DCHECK_LT(ref_count_, kuint32max);
    if (++ref_count_ == 1)
      Traits::Initialize();
  }

  static void Release() {
    base::AutoLock auto_lock(lock_.Get());
    DCHECK_GT(ref_count_, 0U);
    if (--ref_count_ == 0)
      Traits::Shutdown();
  }

  static base::LazyInstance<base::Lock>::Leaky lock_;
  static uint32 ref_count_;
  DISALLOW_COPY_AND_ASSIGN(ScopedInitializationManager);
};

template<class Traits> base::LazyInstance<base::Lock>::Leaky
    ScopedInitializationManager<Traits>::lock_ = LAZY_INSTANCE_INITIALIZER;

template<class Traits> uint32
    ScopedInitializationManager<Traits>::ref_count_ = 0;

}  // namespace chrome_frame

#endif  // CHROME_FRAME_SCOPED_INITIALIZATION_MANAGER_H_
