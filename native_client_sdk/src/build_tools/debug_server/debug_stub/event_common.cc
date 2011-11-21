/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/platform/nacl_sync.h"

/*
 * Define the common gdb_utils IEvent interface to use the NaCl version.
 * IEvent only suports single trigger Signal API.
 */

#if NACL_WINDOWS
# include "native_client/src/shared/platform/win/lock.h"
# include "native_client/src/shared/platform/win/condition_variable.h"
#elif NACL_LINUX || NACL_OSX
# include "native_client/src/shared/platform/linux/lock.h"
# include "native_client/src/shared/platform/linux/condition_variable.h"
#endif

#include "native_client/src/debug_server/port/event.h"

namespace port {

class Event : public IEvent {
 public:
  Event() : signaled_(false) { }
  ~Event() {}

  void Wait() {
    /* Start the wait owning the lock */
    lock_.Acquire();

    /* We can skip this if already signaled */
    while (!signaled_) {
      /* Otherwise, try and wait, which release the lock */
      cond_.Wait(lock_);

      /* We exit the wait owning the lock again. */
    };

    /* Clear the signal then unlock. */
    signaled_ = false;
    lock_.Release();
  }

  void Signal() {
    signaled_ = true;
    cond_.Signal();
  }

 public:
  volatile bool signaled_;
  NaCl::Lock lock_;
  NaCl::ConditionVariable cond_;
};

IEvent* IEvent::Allocate() {
  return new Event;
}

void IEvent::Free(IEvent *ievent) {
  delete static_cast<Event*>(ievent);
}

}  // End of port namespace

