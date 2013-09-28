// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/dispatcher.h"

#include "base/logging.h"

namespace mojo {
namespace system {

MojoResult Dispatcher::Close() {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  is_closed_ = true;
  CancelAllWaitersNoLock();
  return CloseImplNoLock();
}

MojoResult Dispatcher::WriteMessage(const void* bytes,
                                    uint32_t num_bytes,
                                    const MojoHandle* handles,
                                    uint32_t num_handles,
                                    MojoWriteMessageFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return WriteMessageImplNoLock(bytes, num_bytes, handles, num_handles, flags);
}

MojoResult Dispatcher::ReadMessage(void* bytes,
                                   uint32_t* num_bytes,
                                   MojoHandle* handles,
                                   uint32_t* num_handles,
                                   MojoReadMessageFlags flags) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return ReadMessageImplNoLock(bytes, num_bytes, handles, num_handles, flags);
}

MojoResult Dispatcher::AddWaiter(Waiter* waiter,
                                 MojoWaitFlags flags,
                                 MojoResult wake_result) {
  DCHECK_GE(wake_result, 0);

  base::AutoLock locker(lock_);
  if (is_closed_)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return AddWaiterImplNoLock(waiter, flags, wake_result);
}

void Dispatcher::RemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  if (is_closed_)
    return;
  RemoveWaiterImplNoLock(waiter);
}

Dispatcher::Dispatcher()
    : is_closed_(false) {
}

Dispatcher::~Dispatcher() {
  // Make sure that |Close()| was called.
  DCHECK(is_closed_);
}

void Dispatcher::CancelAllWaitersNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

MojoResult Dispatcher::CloseImplNoLock() {
  lock_.AssertAcquired();
  DCHECK(is_closed_);
  // This may not need to do anything. Dispatchers should override this to do
  // any actual close-time cleanup necessary.
  return MOJO_RESULT_OK;
}

MojoResult Dispatcher::WriteMessageImplNoLock(const void* bytes,
                                              uint32_t num_bytes,
                                              const MojoHandle* handles,
                                              uint32_t num_handles,
                                              MojoWriteMessageFlags flags) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, this isn't supported. Only dispatchers for message pipes (with
  // whatever implementation, possibly a proxy) will do something nontrivial.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::ReadMessageImplNoLock(void* bytes,
                                             uint32_t* num_bytes,
                                             MojoHandle* handles,
                                             uint32_t* num_handles,
                                             MojoReadMessageFlags flags) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, this isn't supported. Only dispatchers for message pipes (with
  // whatever implementation, possibly a proxy) will do something nontrivial.
  return MOJO_RESULT_INVALID_ARGUMENT;
}

MojoResult Dispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                           MojoWaitFlags flags,
                                           MojoResult wake_result) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
  return MOJO_RESULT_FAILED_PRECONDITION;
}

void Dispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock_.AssertAcquired();
  DCHECK(!is_closed_);
  // By default, waiting isn't supported. Only dispatchers that can be waited on
  // will do something nontrivial.
}

}  // namespace system
}  // namespace mojo
