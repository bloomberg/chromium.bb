// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/core_impl.h"

#include <vector>

#include "base/logging.h"
#include "base/time/time.h"
#include "mojo/system/constants.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/data_pipe_consumer_dispatcher.h"
#include "mojo/system/data_pipe_producer_dispatcher.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/local_data_pipe.h"
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

CoreImpl::HandleTableEntry::HandleTableEntry()
    : busy(false) {
}

CoreImpl::HandleTableEntry::HandleTableEntry(
    const scoped_refptr<Dispatcher>& dispatcher)
    : dispatcher(dispatcher),
      busy(false) {
}

CoreImpl::HandleTableEntry::~HandleTableEntry() {
  DCHECK(!busy);
}

CoreImpl::CoreImpl()
    : next_handle_(MOJO_HANDLE_INVALID + 1) {
}

CoreImpl::~CoreImpl() {
  // This should usually not be reached (the singleton lives forever), except in
  // tests.
}

MojoHandle CoreImpl::AddDispatcher(
    const scoped_refptr<Dispatcher>& dispatcher) {
  base::AutoLock locker(handle_table_lock_);
  return AddDispatcherNoLock(dispatcher);
}

MojoTimeTicks CoreImpl::GetTimeTicksNow() {
  return base::TimeTicks::Now().ToInternalValue();
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
    if (it->second.busy)
      return MOJO_RESULT_BUSY;
    dispatcher = it->second.dispatcher;
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

MojoResult CoreImpl::CreateMessagePipe(MojoHandle* message_pipe_handle0,
                                       MojoHandle* message_pipe_handle1) {
  if (!VerifyUserPointer<MojoHandle>(message_pipe_handle0, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<MojoHandle>(message_pipe_handle1, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<MessagePipeDispatcher> dispatcher0(new MessagePipeDispatcher());
  scoped_refptr<MessagePipeDispatcher> dispatcher1(new MessagePipeDispatcher());

  MojoHandle h0, h1;
  {
    base::AutoLock locker(handle_table_lock_);

    h0 = AddDispatcherNoLock(dispatcher0);
    if (h0 == MOJO_HANDLE_INVALID)
      return MOJO_RESULT_RESOURCE_EXHAUSTED;

    h1 = AddDispatcherNoLock(dispatcher1);
    if (h1 == MOJO_HANDLE_INVALID) {
      handle_table_.erase(h0);
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    }
  }

  scoped_refptr<MessagePipe> message_pipe(new MessagePipe());
  dispatcher0->Init(message_pipe, 0);
  dispatcher1->Init(message_pipe, 1);

  *message_pipe_handle0 = h0;
  *message_pipe_handle1 = h1;
  return MOJO_RESULT_OK;
}

MojoResult CoreImpl::WriteMessage(MojoHandle message_pipe_handle,
                                  const void* bytes,
                                  uint32_t num_bytes,
                                  const MojoHandle* handles,
                                  uint32_t num_handles,
                                  MojoWriteMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Easy case: not sending any handles.
  if (num_handles == 0)
    return dispatcher->WriteMessage(bytes, num_bytes, NULL, flags);

  // We have to handle |handles| here, since we have to mark them busy in the
  // global handle table. We can't delegate this to the dispatcher, since the
  // handle table lock must be acquired before the dispatcher lock.
  //
  // (This leads to an oddity: |handles|/|num_handles| are always verified for
  // validity, even for dispatchers that don't support |WriteMessage()| and will
  // simply return failure unconditionally. It also breaks the usual
  // left-to-right verification order of arguments.)
  if (!VerifyUserPointer<MojoHandle>(handles, num_handles))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > kMaxMessageNumHandles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  // We'll need to hold on to the dispatchers so that we can pass them on to
  // |WriteMessage()| and also so that we can unlock their locks afterwards
  // without accessing the handle table. These can be dumb pointers, since their
  // entries in the handle table won't get removed (since they'll be marked as
  // busy).
  std::vector<DispatcherTransport> transports(num_handles);

  // When we pass handles, we have to try to take all their dispatchers' locks
  // and mark the handles as busy. If the call succeeds, we then remove the
  // handles from the handle table.
  {
    base::AutoLock locker(handle_table_lock_);

    std::vector<HandleTableEntry*> entries(num_handles);

    // First verify all the handles and get their dispatchers.
    uint32_t i;
    MojoResult error_result = MOJO_RESULT_INTERNAL;
    for (i = 0; i < num_handles; i++) {
      // Sending your own handle is not allowed (and, for consistency, returns
      // "busy").
      if (handles[i] == message_pipe_handle) {
        error_result = MOJO_RESULT_BUSY;
        break;
      }

      HandleTableMap::iterator it = handle_table_.find(handles[i]);
      if (it == handle_table_.end()) {
        error_result = MOJO_RESULT_INVALID_ARGUMENT;
        break;
      }

      entries[i] = &it->second;
      if (entries[i]->busy) {
        error_result = MOJO_RESULT_BUSY;
        break;
      }
      // Note: By marking the handle as busy here, we're also preventing the
      // same handle from being sent multiple times in the same message.
      entries[i]->busy = true;

      // Try to start the transport.
      DispatcherTransport transport =
          Dispatcher::CoreImplAccess::TryStartTransport(
              entries[i]->dispatcher.get());
      if (!transport.is_valid()) {
        // Unset the busy flag (since it won't be unset below).
        entries[i]->busy = false;
        error_result = MOJO_RESULT_BUSY;
        break;
      }

      // Check if the dispatcher is busy (e.g., in a two-phase read/write).
      // (Note that this must be done after the dispatcher's lock is acquired.)
      if (transport.IsBusy()) {
        // Unset the busy flag and end the transport (since it won't be done
        // below).
        entries[i]->busy = false;
        transport.End();
        error_result = MOJO_RESULT_BUSY;
        break;
      }

      // Hang on to the transport (which we'll need to end the transport).
      transports[i] = transport;
    }
    if (i < num_handles) {
      DCHECK_NE(error_result, MOJO_RESULT_INTERNAL);

      // Unset the busy flags and release the locks.
      for (uint32_t j = 0; j < i; j++) {
        DCHECK(entries[j]->busy);
        entries[j]->busy = false;
        transports[j].End();
      }
      return error_result;
    }
  }

  MojoResult rv = dispatcher->WriteMessage(bytes, num_bytes, &transports,
                                           flags);

  // We need to release the dispatcher locks before we take the handle table
  // lock.
  for (uint32_t i = 0; i < num_handles; i++)
    transports[i].End();

  if (rv == MOJO_RESULT_OK) {
    base::AutoLock locker(handle_table_lock_);

    // Succeeded, so the handles should be removed from the handle table. (The
    // transferring to new dispatchers/closing must have already been done.)
    for (uint32_t i = 0; i < num_handles; i++) {
      HandleTableMap::iterator it = handle_table_.find(handles[i]);
      DCHECK(it != handle_table_.end());
      DCHECK(it->second.busy);
      it->second.busy = false;  // For the sake of a |DCHECK()|.
      handle_table_.erase(it);
    }
  } else {
    base::AutoLock locker(handle_table_lock_);

    // Failed, so the handles should go back to their normal state.
    for (uint32_t i = 0; i < num_handles; i++) {
      HandleTableMap::iterator it = handle_table_.find(handles[i]);
      DCHECK(it != handle_table_.end());
      DCHECK(it->second.busy);
      it->second.busy = false;
    }
  }

  return rv;
}

MojoResult CoreImpl::ReadMessage(MojoHandle message_pipe_handle,
                                 void* bytes,
                                 uint32_t* num_bytes,
                                 MojoHandle* handles,
                                 uint32_t* num_handles,
                                 MojoReadMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (num_handles) {
    if (!VerifyUserPointer<uint32_t>(num_handles, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointer<MojoHandle>(handles, *num_handles))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  // Easy case: won't receive any handles.
  if (!num_handles || *num_handles == 0)
    return dispatcher->ReadMessage(bytes, num_bytes, NULL, num_handles, flags);

  std::vector<scoped_refptr<Dispatcher> > dispatchers;
  MojoResult rv = dispatcher->ReadMessage(bytes, num_bytes,
                                          &dispatchers, num_handles,
                                          flags);
  if (!dispatchers.empty()) {
    DCHECK_EQ(rv, MOJO_RESULT_OK);
    DCHECK(num_handles);
    DCHECK_LE(dispatchers.size(), static_cast<size_t>(*num_handles));

    base::AutoLock locker(handle_table_lock_);

    for (size_t i = 0; i < dispatchers.size(); i++) {
      // TODO(vtl): What should we do if we hit the maximum handle table size
      // here? Currently, we'll just fill in those handles with
      // |MOJO_HANDLE_INVALID| (and return success anyway).
      handles[i] = AddDispatcherNoLock(dispatchers[i]);
      LOG_IF(ERROR, handles[i] == MOJO_HANDLE_INVALID)
          << "Failed to add dispatcher (" << dispatchers[i].get() << ")";
    }
  }

  return rv;
}

MojoResult CoreImpl::CreateDataPipe(const MojoCreateDataPipeOptions* options,
                                    MojoHandle* data_pipe_producer_handle,
                                    MojoHandle* data_pipe_consumer_handle) {
  if (options) {
    // The |struct_size| field must be valid to read.
    if (!VerifyUserPointer<uint32_t>(&options->struct_size, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    // And then |options| must point to at least |options->struct_size| bytes.
    if (!VerifyUserPointer<void>(options, options->struct_size))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }
  if (!VerifyUserPointer<MojoHandle>(data_pipe_producer_handle, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<MojoHandle>(data_pipe_consumer_handle, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;

  MojoCreateDataPipeOptions validated_options = { 0 };
  MojoResult result = DataPipe::ValidateOptions(options, &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;

  scoped_refptr<DataPipeProducerDispatcher> producer_dispatcher(
      new DataPipeProducerDispatcher());
  scoped_refptr<DataPipeConsumerDispatcher> consumer_dispatcher(
      new DataPipeConsumerDispatcher());

  MojoHandle producer_handle, consumer_handle;
  {
    base::AutoLock locker(handle_table_lock_);

    producer_handle = AddDispatcherNoLock(producer_dispatcher);
    if (producer_handle == MOJO_HANDLE_INVALID)
      return MOJO_RESULT_RESOURCE_EXHAUSTED;

    consumer_handle = AddDispatcherNoLock(consumer_dispatcher);
    if (consumer_handle == MOJO_HANDLE_INVALID) {
      handle_table_.erase(producer_handle);
      return MOJO_RESULT_RESOURCE_EXHAUSTED;
    }
  }

  scoped_refptr<DataPipe> data_pipe(new LocalDataPipe(validated_options));
  producer_dispatcher->Init(data_pipe);
  consumer_dispatcher->Init(data_pipe);

  *data_pipe_producer_handle = producer_handle;
  *data_pipe_consumer_handle = consumer_handle;
  return MOJO_RESULT_OK;
}

MojoResult CoreImpl::WriteData(MojoHandle data_pipe_producer_handle,
                               const void* elements,
                               uint32_t* num_bytes,
                               MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->WriteData(elements, num_bytes, flags);
}

MojoResult CoreImpl::BeginWriteData(MojoHandle data_pipe_producer_handle,
                                    void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginWriteData(buffer, buffer_num_bytes, flags);
}

MojoResult CoreImpl::EndWriteData(MojoHandle data_pipe_producer_handle,
                                  uint32_t num_bytes_written) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndWriteData(num_bytes_written);
}

MojoResult CoreImpl::ReadData(MojoHandle data_pipe_consumer_handle,
                              void* elements,
                              uint32_t* num_bytes,
                              MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->ReadData(elements, num_bytes, flags);
}

MojoResult CoreImpl::BeginReadData(MojoHandle data_pipe_consumer_handle,
                                   const void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginReadData(buffer, buffer_num_bytes, flags);
}

MojoResult CoreImpl::EndReadData(MojoHandle data_pipe_consumer_handle,
                                 uint32_t num_bytes_read) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher.get())
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndReadData(num_bytes_read);
}

MojoResult CoreImpl::CreateSharedBuffer(
    const MojoCreateSharedBufferOptions* options,
    uint64_t* num_bytes,
    MojoHandle* shared_buffer_handle) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult CoreImpl::DuplicateSharedBuffer(
    MojoHandle shared_buffer_handle,
    const MojoDuplicateSharedBufferOptions* options,
    MojoHandle* new_shared_buffer_handle) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult CoreImpl::MapBuffer(MojoHandle buffer_handle,
                               uint64_t offset,
                               uint64_t num_bytes,
                               void** buffer,
                               MojoMapBufferFlags flags) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return MOJO_RESULT_UNIMPLEMENTED;
}

MojoResult CoreImpl::UnmapBuffer(void* buffer) {
  // TODO(vtl)
  NOTIMPLEMENTED();
  return MOJO_RESULT_UNIMPLEMENTED;
}

scoped_refptr<Dispatcher> CoreImpl::GetDispatcher(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return NULL;

  base::AutoLock locker(handle_table_lock_);
  HandleTableMap::iterator it = handle_table_.find(handle);
  if (it == handle_table_.end())
    return NULL;

  return it->second.dispatcher;
}

MojoHandle CoreImpl::AddDispatcherNoLock(
    const scoped_refptr<Dispatcher>& dispatcher) {
  handle_table_lock_.AssertAcquired();
  DCHECK_NE(next_handle_, MOJO_HANDLE_INVALID);

  if (!dispatcher.get() || handle_table_.size() >= kMaxHandleTableSize)
    return MOJO_HANDLE_INVALID;

  // TODO(vtl): Maybe we want to do something different/smarter. (Or maybe try
  // assigning randomly?)
  while (handle_table_.find(next_handle_) != handle_table_.end()) {
    next_handle_++;
    if (next_handle_ == MOJO_HANDLE_INVALID)
      next_handle_++;
  }

  MojoHandle new_handle = next_handle_;
  handle_table_[new_handle] = HandleTableEntry(dispatcher);

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
    scoped_refptr<Dispatcher> dispatcher = GetDispatcher(handles[i]);
    if (!dispatcher.get())
      return MOJO_RESULT_INVALID_ARGUMENT;
    dispatchers.push_back(dispatcher);
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
