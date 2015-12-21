// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/core.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/stack_container.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/async_waiter.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe.h"
#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"
#include "mojo/edk/system/data_pipe_producer_dispatcher.h"
#include "mojo/edk/system/dispatcher.h"
#include "mojo/edk/system/handle_signals_state.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/shared_buffer_dispatcher.h"
#include "mojo/edk/system/wait_set_dispatcher.h"
#include "mojo/edk/system/waiter.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/cpp/system/macros.h"

namespace mojo {
namespace edk {

// Implementation notes
//
// Mojo primitives are implemented by the singleton |Core| object. Most calls
// are for a "primary" handle (the first argument). |Core::GetDispatcher()| is
// used to look up a |Dispatcher| object for a given handle. That object
// implements most primitives for that object. The wait primitives are not
// attached to objects and are implemented by |Core| itself.
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
// The |Dispatcher| then adds the |Waiter| to an |AwakableList| that's either
// owned by that |Dispatcher| (see |SimpleDispatcher|) or by a secondary object
// (e.g., |MessagePipe|). To signal/wake a |Waiter|, the object in question --
// either a |SimpleDispatcher| or a secondary object -- talks to its
// |AwakableList|.

// Thread-safety notes
//
// Mojo primitives calls are thread-safe. We achieve this with relatively
// fine-grained locking. There is a global handle table lock. This lock should
// be held as briefly as possible (TODO(vtl): a future improvement would be to
// switch it to a reader-writer lock). Each |Dispatcher| object then has a lock
// (which subclasses can use to protect their data).
//
// The lock ordering is as follows:
//   1. global handle table lock, global mapping table lock
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

// TODO(vtl): This should take a |scoped_ptr<PlatformSupport>| as a parameter.
Core::Core(PlatformSupport* platform_support)
    : platform_support_(platform_support) {
}

Core::~Core() {
}

MojoHandle Core::AddDispatcher(const scoped_refptr<Dispatcher>& dispatcher) {
  base::AutoLock locker(handle_table_lock_);
  return handle_table_.AddDispatcher(dispatcher);
}

scoped_refptr<Dispatcher> Core::GetDispatcher(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return nullptr;

  base::AutoLock locker(handle_table_lock_);
  return handle_table_.GetDispatcher(handle);
}

MojoResult Core::AsyncWait(MojoHandle handle,
                           MojoHandleSignals signals,
                           const base::Callback<void(MojoResult)>& callback) {
  scoped_refptr<Dispatcher> dispatcher = GetDispatcher(handle);
  DCHECK(dispatcher);

  scoped_ptr<AsyncWaiter> waiter = make_scoped_ptr(new AsyncWaiter(callback));
  MojoResult rv = dispatcher->AddAwakable(waiter.get(), signals, 0, nullptr);
  if (rv == MOJO_RESULT_OK)
    ignore_result(waiter.release());
  return rv;
}

MojoTimeTicks Core::GetTimeTicksNow() {
  return base::TimeTicks::Now().ToInternalValue();
}

MojoResult Core::Close(MojoHandle handle) {
  if (handle == MOJO_HANDLE_INVALID)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> dispatcher;
  {
    base::AutoLock locker(handle_table_lock_);
    MojoResult result =
        handle_table_.GetAndRemoveDispatcher(handle, &dispatcher);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  // The dispatcher doesn't have a say in being closed, but gets notified of it.
  // Note: This is done outside of |handle_table_lock_|. As a result, there's a
  // race condition that the dispatcher must handle; see the comment in
  // |Dispatcher| in dispatcher.h.
  return dispatcher->Close();
}

MojoResult Core::Wait(MojoHandle handle,
                      MojoHandleSignals signals,
                      MojoDeadline deadline,
                      MojoHandleSignalsState* signals_state) {
  uint32_t unused = static_cast<uint32_t>(-1);
  HandleSignalsState hss;
  MojoResult rv = WaitManyInternal(&handle, &signals, 1, deadline, &unused,
                                   signals_state ? &hss : nullptr);
  if (rv != MOJO_RESULT_INVALID_ARGUMENT && signals_state)
    *signals_state = hss;
  return rv;
}

MojoResult Core::WaitMany(const MojoHandle* handles,
                          const MojoHandleSignals* signals,
                          uint32_t num_handles,
                          MojoDeadline deadline,
                          uint32_t* result_index,
                          MojoHandleSignalsState* signals_state) {
  if (num_handles < 1)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_handles > GetConfiguration().max_wait_many_num_handles)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  uint32_t index = static_cast<uint32_t>(-1);
  MojoResult rv;
  if (!signals_state) {
    rv = WaitManyInternal(handles, signals, num_handles, deadline, &index,
                          nullptr);
  } else {
    // Note: The |reinterpret_cast| is safe, since |HandleSignalsState| is a
    // subclass of |MojoHandleSignalsState| that doesn't add any data members.
    rv = WaitManyInternal(handles, signals, num_handles, deadline, &index,
                          reinterpret_cast<HandleSignalsState*>(signals_state));
  }
  if (index != static_cast<uint32_t>(-1) && result_index)
    *result_index = index;
  return rv;
}

MojoResult Core::CreateWaitSet(MojoHandle* wait_set_handle) {
  if (!wait_set_handle)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<WaitSetDispatcher> dispatcher = new WaitSetDispatcher();
  MojoHandle h = AddDispatcher(dispatcher);
  if (h == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *wait_set_handle = h;
  return MOJO_RESULT_OK;
}

MojoResult Core::AddHandle(MojoHandle wait_set_handle,
                           MojoHandle handle,
                           MojoHandleSignals signals) {
  scoped_refptr<Dispatcher> wait_set_dispatcher(GetDispatcher(wait_set_handle));
  if (!wait_set_dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return wait_set_dispatcher->AddWaitingDispatcher(dispatcher, signals, handle);
}

MojoResult Core::RemoveHandle(MojoHandle wait_set_handle,
                              MojoHandle handle) {
  scoped_refptr<Dispatcher> wait_set_dispatcher(GetDispatcher(wait_set_handle));
  if (!wait_set_dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return wait_set_dispatcher->RemoveWaitingDispatcher(dispatcher);
}

MojoResult Core::GetReadyHandles(MojoHandle wait_set_handle,
                                 uint32_t* count,
                                 MojoHandle* handles,
                                 MojoResult* results,
                                 MojoHandleSignalsState* signals_state) {
  if (!handles || !count || !(*count) || !results)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_refptr<Dispatcher> wait_set_dispatcher(GetDispatcher(wait_set_handle));
  if (!wait_set_dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  DispatcherVector awoken_dispatchers;
  base::StackVector<uintptr_t, 16> contexts;
  contexts->assign(*count, MOJO_HANDLE_INVALID);

  MojoResult result = wait_set_dispatcher->GetReadyDispatchers(
      count, &awoken_dispatchers, results, contexts->data());

  if (result == MOJO_RESULT_OK) {
    for (size_t i = 0; i < *count; i++) {
      handles[i] = static_cast<MojoHandle>(contexts[i]);
      if (signals_state)
        signals_state[i] = awoken_dispatchers[i]->GetHandleSignalsState();
    }
  }

  return result;
}

MojoResult Core::CreateMessagePipe(
    const MojoCreateMessagePipeOptions* options,
    MojoHandle* message_pipe_handle0,
    MojoHandle* message_pipe_handle1) {
  CHECK(message_pipe_handle0);
  CHECK(message_pipe_handle1);
  MojoCreateMessagePipeOptions validated_options = {};
  MojoResult result =
      MessagePipeDispatcher::ValidateCreateOptions(options, &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;

  scoped_refptr<MessagePipeDispatcher> dispatcher0 =
      MessagePipeDispatcher::Create(validated_options);
  scoped_refptr<MessagePipeDispatcher> dispatcher1 =
      MessagePipeDispatcher::Create(validated_options);

  std::pair<MojoHandle, MojoHandle> handle_pair;
  {
    base::AutoLock locker(handle_table_lock_);
    handle_pair = handle_table_.AddDispatcherPair(dispatcher0, dispatcher1);
  }
  if (handle_pair.first == MOJO_HANDLE_INVALID) {
    DCHECK_EQ(handle_pair.second, MOJO_HANDLE_INVALID);
    LOG(ERROR) << "Handle table full";
    dispatcher0->Close();
    dispatcher1->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  if (validated_options.flags &
          MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE) {
    ScopedPlatformHandle server_handle, client_handle;
#if defined(OS_WIN)
    internal::g_broker->CreatePlatformChannelPair(&server_handle,
                                                  &client_handle);
#else
    PlatformChannelPair channel_pair;
    server_handle = channel_pair.PassServerHandle();
    client_handle = channel_pair.PassClientHandle();
#endif
    dispatcher0->Init(std::move(server_handle), nullptr, 0u, nullptr, 0u,
                      nullptr, nullptr);
    dispatcher1->Init(std::move(client_handle), nullptr, 0u, nullptr, 0u,
                      nullptr, nullptr);
  } else {
    uint64_t pipe_id = 0;
    // route_id 0 is used internally in RoutedRawChannel. See kInternalRouteId
    // in routed_raw_channel.cc.
    // route_id 1 is used by broker communication. See kBrokerRouteId in
    // broker_messages.h.
    while (pipe_id < 2)
      pipe_id = base::RandUint64();
    dispatcher0->InitNonTransferable(pipe_id);
    dispatcher1->InitNonTransferable(pipe_id);
  }

  *message_pipe_handle0 = handle_pair.first;
  *message_pipe_handle1 = handle_pair.second;
  return MOJO_RESULT_OK;
}

// Implementation note: To properly cancel waiters and avoid other races, this
// does not transfer dispatchers from one handle to another, even when sending a
// message in-process. Instead, it must transfer the "contents" of the
// dispatcher to a new dispatcher, and then close the old dispatcher. If this
// isn't done, in the in-process case, calls on the old handle may complete
// after the the message has been received and a new handle created (and
// possibly even after calls have been made on the new handle).
MojoResult Core::WriteMessage(MojoHandle message_pipe_handle,
                              const void* bytes,
                              uint32_t num_bytes,
                              const MojoHandle* handles,
                              uint32_t num_handles,
                              MojoWriteMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Easy case: not sending any handles.
  if (num_handles == 0)
    return dispatcher->WriteMessage(bytes, num_bytes, nullptr, flags);

  // We have to handle |handles| here, since we have to mark them busy in the
  // global handle table. We can't delegate this to the dispatcher, since the
  // handle table lock must be acquired before the dispatcher lock.
  //
  // (This leads to an oddity: |handles|/|num_handles| are always verified for
  // validity, even for dispatchers that don't support |WriteMessage()| and will
  // simply return failure unconditionally. It also breaks the usual
  // left-to-right verification order of arguments.)
  if (num_handles > GetConfiguration().max_message_num_handles)
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
    MojoResult result = handle_table_.MarkBusyAndStartTransport(
        message_pipe_handle, handles, num_handles, &transports);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  MojoResult rv =
      dispatcher->WriteMessage(bytes, num_bytes, &transports, flags);

  // We need to release the dispatcher locks before we take the handle table
  // lock.
  for (uint32_t i = 0; i < num_handles; i++)
    transports[i].End();

  {
    base::AutoLock locker(handle_table_lock_);
    if (rv == MOJO_RESULT_OK) {
      handle_table_.RemoveBusyHandles(handles, num_handles);
    } else {
      handle_table_.RestoreBusyHandles(handles, num_handles);
    }
  }

  return rv;
}

MojoResult Core::ReadMessage(MojoHandle message_pipe_handle,
                             void* bytes,
                             uint32_t* num_bytes,
                             MojoHandle* handles,
                             uint32_t* num_handles,
                             MojoReadMessageFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(message_pipe_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  MojoResult rv;
  uint32_t num_handles_value = num_handles ? *num_handles : 0;
  if (num_handles_value == 0) {
    // Easy case: won't receive any handles.
    rv = dispatcher->ReadMessage(bytes, num_bytes, nullptr, &num_handles_value,
                                 flags);
  } else {
    DispatcherVector dispatchers;
    rv = dispatcher->ReadMessage(bytes, num_bytes, &dispatchers,
                                 &num_handles_value, flags);
    if (!dispatchers.empty()) {
      DCHECK_EQ(rv, MOJO_RESULT_OK);
      DCHECK(num_handles);
      DCHECK_LE(dispatchers.size(), static_cast<size_t>(num_handles_value));

      bool success;
      {
        base::AutoLock locker(handle_table_lock_);
        success = handle_table_.AddDispatcherVector(dispatchers, handles);
      }
      if (!success) {
        LOG(ERROR) << "Received message with " << dispatchers.size()
                   << " handles, but handle table full";
        // Close dispatchers (outside the lock).
        for (size_t i = 0; i < dispatchers.size(); i++) {
          if (dispatchers[i])
            dispatchers[i]->Close();
        }
        if (rv == MOJO_RESULT_OK)
          rv = MOJO_RESULT_RESOURCE_EXHAUSTED;
      }
    }
  }

  if (num_handles)
    *num_handles = num_handles_value;
  return rv;
}

MojoResult Core::CreateDataPipe(
    const MojoCreateDataPipeOptions* options,
    MojoHandle* data_pipe_producer_handle,
    MojoHandle* data_pipe_consumer_handle) {
  MojoCreateDataPipeOptions validated_options = {};
  MojoResult result =
      DataPipe::ValidateCreateOptions(options, &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;

  scoped_refptr<DataPipeProducerDispatcher> producer_dispatcher =
      DataPipeProducerDispatcher::Create(validated_options);
  scoped_refptr<DataPipeConsumerDispatcher> consumer_dispatcher =
      DataPipeConsumerDispatcher::Create(validated_options);

  std::pair<MojoHandle, MojoHandle> handle_pair;
  {
    base::AutoLock locker(handle_table_lock_);
    handle_pair = handle_table_.AddDispatcherPair(producer_dispatcher,
                                                  consumer_dispatcher);
  }
  if (handle_pair.first == MOJO_HANDLE_INVALID) {
    DCHECK_EQ(handle_pair.second, MOJO_HANDLE_INVALID);
    LOG(ERROR) << "Handle table full";
    producer_dispatcher->Close();
    consumer_dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }
  DCHECK_NE(handle_pair.second, MOJO_HANDLE_INVALID);

  ScopedPlatformHandle server_handle, client_handle;
#if defined(OS_WIN)
  internal::g_broker->CreatePlatformChannelPair(&server_handle, &client_handle);
#else
  PlatformChannelPair channel_pair;
  server_handle = channel_pair.PassServerHandle();
  client_handle = channel_pair.PassClientHandle();
#endif
  producer_dispatcher->Init(std::move(server_handle), nullptr, 0u);
  consumer_dispatcher->Init(std::move(client_handle), nullptr, 0u);

  *data_pipe_producer_handle = handle_pair.first;
  *data_pipe_consumer_handle = handle_pair.second;
  return MOJO_RESULT_OK;
}

MojoResult Core::WriteData(MojoHandle data_pipe_producer_handle,
                           const void* elements,
                           uint32_t* num_bytes,
                           MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->WriteData(elements, num_bytes, flags);
}

MojoResult Core::BeginWriteData(MojoHandle data_pipe_producer_handle,
                                void** buffer,
                                uint32_t* buffer_num_bytes,
                                MojoWriteDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginWriteData(buffer, buffer_num_bytes, flags);
}

MojoResult Core::EndWriteData(MojoHandle data_pipe_producer_handle,
                              uint32_t num_bytes_written) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_producer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndWriteData(num_bytes_written);
}

MojoResult Core::ReadData(MojoHandle data_pipe_consumer_handle,
                          void* elements,
                          uint32_t* num_bytes,
                          MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->ReadData(elements, num_bytes, flags);
}

MojoResult Core::BeginReadData(MojoHandle data_pipe_consumer_handle,
                               const void** buffer,
                               uint32_t* buffer_num_bytes,
                               MojoReadDataFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->BeginReadData(buffer, buffer_num_bytes, flags);
}

MojoResult Core::EndReadData(MojoHandle data_pipe_consumer_handle,
                             uint32_t num_bytes_read) {
  scoped_refptr<Dispatcher> dispatcher(
      GetDispatcher(data_pipe_consumer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  return dispatcher->EndReadData(num_bytes_read);
}

MojoResult Core::CreateSharedBuffer(
    const MojoCreateSharedBufferOptions* options,
    uint64_t num_bytes,
    MojoHandle* shared_buffer_handle) {
  MojoCreateSharedBufferOptions validated_options = {};
  MojoResult result = SharedBufferDispatcher::ValidateCreateOptions(
      options, &validated_options);
  if (result != MOJO_RESULT_OK)
    return result;

  scoped_refptr<SharedBufferDispatcher> dispatcher;
  result = SharedBufferDispatcher::Create(platform_support_, validated_options,
                                          num_bytes, &dispatcher);
  if (result != MOJO_RESULT_OK) {
    DCHECK(!dispatcher);
    return result;
  }

  *shared_buffer_handle = AddDispatcher(dispatcher);
  if (*shared_buffer_handle == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  return MOJO_RESULT_OK;
}

MojoResult Core::DuplicateBufferHandle(
    MojoHandle buffer_handle,
    const MojoDuplicateBufferHandleOptions* options,
    MojoHandle* new_buffer_handle) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(buffer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  // Don't verify |options| here; that's the dispatcher's job.
  scoped_refptr<Dispatcher> new_dispatcher;
  MojoResult result =
      dispatcher->DuplicateBufferHandle(options, &new_dispatcher);
  if (result != MOJO_RESULT_OK)
    return result;

  *new_buffer_handle = AddDispatcher(new_dispatcher);
  if (*new_buffer_handle == MOJO_HANDLE_INVALID) {
    LOG(ERROR) << "Handle table full";
    dispatcher->Close();
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  return MOJO_RESULT_OK;
}

MojoResult Core::MapBuffer(MojoHandle buffer_handle,
                           uint64_t offset,
                           uint64_t num_bytes,
                           void** buffer,
                           MojoMapBufferFlags flags) {
  scoped_refptr<Dispatcher> dispatcher(GetDispatcher(buffer_handle));
  if (!dispatcher)
    return MOJO_RESULT_INVALID_ARGUMENT;

  scoped_ptr<PlatformSharedBufferMapping> mapping;
  MojoResult result = dispatcher->MapBuffer(offset, num_bytes, flags, &mapping);
  if (result != MOJO_RESULT_OK)
    return result;

  DCHECK(mapping);
  void* address = mapping->GetBase();
  {
    base::AutoLock locker(mapping_table_lock_);
    result = mapping_table_.AddMapping(std::move(mapping));
  }
  if (result != MOJO_RESULT_OK)
    return result;

  *buffer = address;
  return MOJO_RESULT_OK;
}

MojoResult Core::UnmapBuffer(void* buffer) {
  base::AutoLock locker(mapping_table_lock_);
  return mapping_table_.RemoveMapping(buffer);
}

// Note: We allow |handles| to repeat the same handle multiple times, since
// different flags may be specified.
// TODO(vtl): This incurs a performance cost in |Remove()|. Analyze this
// more carefully and address it if necessary.
MojoResult Core::WaitManyInternal(const MojoHandle* handles,
                                  const MojoHandleSignals* signals,
                                  uint32_t num_handles,
                                  MojoDeadline deadline,
                                  uint32_t* result_index,
                                  HandleSignalsState* signals_states) {
  CHECK(handles);
  CHECK(signals);
  DCHECK_GT(num_handles, 0u);
  if (result_index) {
    DCHECK_EQ(*result_index, static_cast<uint32_t>(-1));
  }

  DispatcherVector dispatchers;
  dispatchers.reserve(num_handles);
  for (uint32_t i = 0; i < num_handles; i++) {
    scoped_refptr<Dispatcher> dispatcher = GetDispatcher(handles[i]);
    if (!dispatcher) {
      if (result_index)
        *result_index = i;
      return MOJO_RESULT_INVALID_ARGUMENT;
    }
    dispatchers.push_back(dispatcher);
  }

  // TODO(vtl): Should make the waiter live (permanently) in TLS.
  Waiter waiter;
  waiter.Init();

  uint32_t i;
  MojoResult rv = MOJO_RESULT_OK;
  for (i = 0; i < num_handles; i++) {
    rv = dispatchers[i]->AddAwakable(
        &waiter, signals[i], i, signals_states ? &signals_states[i] : nullptr);
    if (rv != MOJO_RESULT_OK) {
      if (result_index)
        *result_index = i;
      break;
    }
  }
  uint32_t num_added = i;

  if (rv == MOJO_RESULT_ALREADY_EXISTS) {
    rv = MOJO_RESULT_OK;  // The i-th one is already "triggered".
  } else if (rv == MOJO_RESULT_OK) {
    uintptr_t uintptr_result = *result_index;
    rv = waiter.Wait(deadline, &uintptr_result);
    *result_index = static_cast<uint32_t>(uintptr_result);
  }

  // Make sure no other dispatchers try to wake |waiter| for the current
  // |Wait()|/|WaitMany()| call. (Only after doing this can |waiter| be
  // destroyed, but this would still be required if the waiter were in TLS.)
  for (i = 0; i < num_added; i++) {
    dispatchers[i]->RemoveAwakable(
        &waiter, signals_states ? &signals_states[i] : nullptr);
  }
  if (signals_states) {
    for (; i < num_handles; i++)
      signals_states[i] = dispatchers[i]->GetHandleSignalsState();
  }

  return rv;
}

}  // namespace edk
}  // namespace mojo
