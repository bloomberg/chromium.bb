// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_impl.h"

#include <vector>

#include "base/logging.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/limits.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_pipe.h"
#include "mojo/system/message_pipe_dispatcher.h"
#include "mojo/system/waiter.h"

namespace mojo {
namespace system {

// Implementation notes
//
// Mojo primitives are implemented by the singleton |CoreImpl| object. Most
// calls are for a "primary" handle (the first argument).
// |CoreImpl::GetDispatcher()| is used to look up a |Dispatcher| object for a
// given handle. That object implements most primitives for that object. The
// wait primitives are not attached to objects and are implemented by |CoreImpl|
// itself.
//
// Some objects have multiple handles associated to them, e.g., message pipes
// (which have two). In such a case, there is still a |Dispatcher| (e.g.,
// |MessagePipeDispatcher|) for each handle, with each handle having a strong
// reference to the common "secondary" object (e.g., |MessagePipe|). This
// secondary object does NOT have any references to the |Dispatcher|s (even if
// it did, it wouldn't be able to do anything with them due to lock order
// requirements -- see below).
//
// Waiting is implemented by having the thread that wants to wait call the
// |Dispatcher|s for the handles that it wants to wait on with a |Waiter|
// object; this |Waiter| object may be created on the stack of that thread or be
// kept in thread local storage for that thread (TODO(vtl): future improvement).
// The |Dispatcher| then adds the |Waiter| to a |WaiterList| that's either owned
// by that |Dispatcher| (see |SimpleDispatcher|) or by a secondary object (e.g.,
// |MessagePipe|). To signal/wake a |Waiter|, the object in question -- either a
// |SimpleDispatcher| or a secondary object -- talks to its |WaiterList|.

// Thread-safety notes
//
// Mojo primitives calls are thread-safe. We achieve this with relatively
// fine-grained locking. There is a global handle table lock. This lock should
// be held as briefly as possible (TODO(vtl): a future improvement would be to
// switch it to a reader-writer lock). Each |Dispatcher| object then has a lock
// (which subclasses can use to protect their data).
//
// The lock ordering is as follows:
//   1. global handle table lock
//   2. |Dispatcher| locks
//   3. secondary object locks
//   ...
//   INF. |Waiter| locks
//
// Notes:
//    - While holding a |Dispatcher| lock, you may not unconditionally attempt
//      to take another |Dispatcher| lock. (This has consequences on the
//      concurrency semantics of |MojoWriteMessage()| when passing handles.)
//      Doing so would lead to deadlock.
//    - Locks at the "INF" level may not have any locks taken while they are
//      held.

// static
CoreImpl* CoreImpl::singleton_ = NULL;

// static
void CoreImpl::Init() {
  CHECK(!singleton_);
  singleton_ = new CoreImpl();
}

MojoResult CoreImpl::Close(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> dispatcher;
  {
    base::AutoLock locker(handle_table_lock_);
    HandleTableMap::iterator it = handle_table_.find(handle);
    if (it == handle_table_.end())
      return MOJO_RESULT_INVALID_ARGUMENT;
    dispatcher = it->second;
    handle_table_.erase(it);
  }

  // The dispatcher doesn't have a say in being closed, but gets notified of it.
  // Note: This is done outside of |handle_table_lock_|. As a result, there's a
  // race condition that the dispatcher must handle; see the comment in
  // |Dispatcher| in dispatcher.h.
  return dispatcher->Close();
}

MojoResult CoreImpl::Wait(MojoHandle handle,
                          MojoWaitFlags flags,
                          MojoDeadline deadline) {
  return WaitManyInternal(&handle, &flags, 1, deadline);
}

MojoResult CoreImpl::WaitMany(const MojoHandle* handles,
                              const MojoWaitFlags* flags,
                              uint32_t num_handles,
                              MojoDeadline deadline) {
  if (!VerifyUserPointer<MojoHandle>(handles, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<MojoWaitFlags>(flags, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles < 1)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > kMaxWaitManyNumHandles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  return WaitManyInternal(handles, flags, num_handles, deadline);
}

MojoResult CoreImpl::CreateMessagePipe(MojoHandle* handle_0,
                                       MojoHandle* handle_1) {
  scoped_refptr<MessagePipeDispatcher> dispatcher_0(
      new MessagePipeDispatcher());
  scoped_refptr<MessagePipeDispatcher> dispatcher_1(
      new MessagePipeDispatcher());

  MojoHandle h0, h1;
  {
    base::AutoLock locker(handle_table_lock_);

    h0 = AddDispatcherNoLock(dispatcher_0);
    if (h0 == MOJO_HANDLE_INVALID)
      return MOJO_RESULT_RESOURCE_EXHAUSTED;

    h1 = AddDispatcherNoLock(dispatcher_1);
    if (h1 == MOJO_HANDLE_INVALID) {
      handle_table_.erase(h0);
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    }
  }

  scoped_refptr<MessagePipe> message_pipe(new MessagePipe());
  dispatcher_0->Init(message_pipe, 0);
  dispatcher_1->Init(message_pipe, 1);

  *handle_0 = h0;
  *handle_1 = h1;
  return MOJO_RESULT_OK;
}

MojoResult CoreImpl::WriteMessage(
    MojoHandle handle,
    const void* bytes, uint32_t num_bytes,
    const MojoHandle* handles, uint32_t num_handles,
    MojoWriteMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->WriteMessage(bytes, num_bytes,
                                  handles, num_handles,
                                  flags);
}

MojoResult CoreImpl::ReadMessage(
    MojoHandle handle,
    void* bytes, uint32_t* num_bytes,
    MojoHandle* handles, uint32_t* num_handles,
    MojoReadMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->ReadMessage(bytes, num_bytes,
                                 handles, num_handles,
                                 flags);
}

CoreImpl::CoreImpl()
    : next_handle_(MOJO_HANDLE_INVALID + 1) {
}

CoreImpl::~CoreImpl() {
  // This should usually not be reached (the singleton lives forever), except
  // in tests.
}

scoped_refptr<Dispatcher> CoreImpl::GetDispatcher(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return NULL;

  base::AutoLock locker(handle_table_lock_);
  HandleTableMap::iterator it = handle_table_.find(handle);
  if (it == handle_table_.end())
    return NULL;

  return it->second;
}

MojoHandle CoreImpl::AddDispatcherNoLock(scoped_refptr<Dispatcher> dispatcher) {
  DCHECK(dispatcher.get());
  handle_table_lock_.AssertAcquired();
  DCHECK_NE(next_handle_, MOJO_HANDLE_INVALID);

  if (handle_table_.size() >= kMaxHandleTableSize)
    return MOJO_HANDLE_INVALID;

  // TODO(vtl): Maybe we want to do something different/smarter. (Or maybe try
  // assigning randomly?)
  while (handle_table_.find(next_handle_) != handle_table_.end()) {
    next_handle_++;
    if (next_handle_ == MOJO_HANDLE_INVALID)
      next_handle_++;
  }

  MojoHandle new_handle = next_handle_;
  handle_table_[new_handle] = dispatcher;

  next_handle_++;
  if (next_handle_ == MOJO_HANDLE_INVALID)
    next_handle_++;

  return new_handle;
}

// Note: We allow |handles| to repeat the same handle multiple times, since
// different flags may be specified.
// TODO(vtl): This incurs a performance cost in |RemoveWaiter()|. Analyze this
// more carefully and address it if necessary.
MojoResult CoreImpl::WaitManyInternal(const MojoHandle* handles,
                                      const MojoWaitFlags* flags,
                                      uint32_t num_handles,
                                      MojoDeadline deadline) {
  DCHECK_GT(num_handles, 0u);

  std::vector<scoped_refptr<Dispatcher> > dispatchers;
  dispatchers.reserve(num_handles);
  for (uint32_t i = 0; i < num_handles; i++) {
    scoped_refptr<Dispatcher> d = GetDispatcher(handles[i]);
    if (!d.get())
      return MOJO_RESULT_INVALID_ARGUMENT;
    dispatchers.push_back(d);
  }

  // TODO(vtl): Should make the waiter live (permanently) in TLS.
  Waiter waiter;
  waiter.Init();

  uint32_t i;
  MojoResult rv = MOJO_RESULT_OK;
  for (i = 0; i < num_handles; i++) {
    rv = dispatchers[i]->AddWaiter(&waiter,
                                   flags[i],
                                   static_cast<MojoResult>(i));
    if (rv != MOJO_RESULT_OK)
      break;
  }
  uint32_t num_added = i;

  if (rv == MOJO_RESULT_ALREADY_EXISTS)
    rv = static_cast<MojoResult>(i);  // The i-th one is already "triggered".
  else if (rv == MOJO_RESULT_OK)
    rv = waiter.Wait(deadline);

  // Make sure no other dispatchers try to wake |waiter| for the current
  // |Wait()|/|WaitMany()| call. (Only after doing this can |waiter| be
  // destroyed, but this would still be required if the waiter were in TLS.)
  for (i = 0; i < num_added; i++)
    dispatchers[i]->RemoveWaiter(&waiter);

  return rv;
}

}  // namespace system
}  // namespace mojo
