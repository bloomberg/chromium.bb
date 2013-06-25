/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef LIBRARIES_SDK_UTIL_REF_OBJECT
#define LIBRARIES_SDK_UTIL_REF_OBJECT

#include <stdlib.h>
#include "pthread.h"

#include "sdk_util/atomicops.h"

class ScopedRefBase;

/*
 * RefObject
 *
 * A reference counted object with a lock.  The lock protects the data within
 * the object, but not the reference counting which happens through atomic
 * operations.  RefObjects should only be handled by ScopedRef objects.
 *
 * When the object is first created, it has a reference count of zero.  It's
 * first incremented when it gets assigned to a ScopedRef.  When the last
 * ScopedRef is reset or destroyed the object will get released.
 */

class RefObject {
 public:
  RefObject() {
    ref_count_ = 0;
    pthread_mutex_init(&lock_, NULL);
  }

  /*
   * RefCount
   *
   * RefCount returns an instantaneous snapshot of the RefCount, which may
   * change immediately after it is fetched.  This is only stable when all
   * pointers to the object are scoped pointers (ScopedRef), and the value
   * is one implying the current thread is the only one, with visibility to
   * the object.
   */
  int RefCount() const { return ref_count_; }

 protected:
  void Acquire() {
    AtomicAddFetch(&ref_count_, 1);
  }

  bool Release() {
    Atomic32 val = AtomicAddFetch(&ref_count_, -1);
    if (val == 0) {
      Destroy();
      delete this;
      return false;
    }
    return true;
  }

  virtual ~RefObject() {
    pthread_mutex_destroy(&lock_);
  }

  // Override to clean up object when last reference is released.
  virtual void Destroy() {}
  pthread_mutex_t lock_;

 private:
  Atomic32 ref_count_;

  friend class ScopedRefBase;
};

#endif  // LIBRARIES_SDK_UTIL_REF_OBJECT

