// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_producer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe.h"

namespace mojo {
namespace edk {

void DataPipeProducerDispatcher::Init(
    ScopedPlatformHandle message_pipe,
    char* serialized_write_buffer, size_t serialized_write_buffer_size) {
  if (message_pipe.is_valid()) {
    channel_ = RawChannel::Create(std::move(message_pipe));
    channel_->SetSerializedData(
        nullptr, 0u, serialized_write_buffer, serialized_write_buffer_size,
        nullptr, nullptr);
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&DataPipeProducerDispatcher::InitOnIO, this));
  } else {
    error_ = true;
  }
}

void DataPipeProducerDispatcher::InitOnIO() {
  base::AutoLock locker(lock());
  if (channel_)
    channel_->Init(this);
}

void DataPipeProducerDispatcher::CloseOnIO() {
  base::AutoLock locker(lock());
  if (channel_) {
    channel_->Shutdown();
    channel_ = nullptr;
  }
}

Dispatcher::Type DataPipeProducerDispatcher::GetType() const {
  return Type::DATA_PIPE_PRODUCER;
}

scoped_refptr<DataPipeProducerDispatcher>
DataPipeProducerDispatcher::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  MojoCreateDataPipeOptions options;
  ScopedPlatformHandle shared_memory_handle;
  size_t shared_memory_size = 0;
  ScopedPlatformHandle platform_handle =
      DataPipe::Deserialize(source, size, platform_handles, &options,
                            &shared_memory_handle, &shared_memory_size);

  scoped_refptr<DataPipeProducerDispatcher> rv(Create(options));

  char* serialized_write_buffer = nullptr;
  size_t serialized_write_buffer_size = 0;
  scoped_refptr<PlatformSharedBuffer> shared_buffer;
  scoped_ptr<PlatformSharedBufferMapping> mapping;
  if (shared_memory_size) {
    shared_buffer = internal::g_platform_support->CreateSharedBufferFromHandle(
        shared_memory_size, std::move(shared_memory_handle));
    mapping = shared_buffer->Map(0, shared_memory_size);
    serialized_write_buffer = static_cast<char*>(mapping->GetBase());
    serialized_write_buffer_size = shared_memory_size;
  }

  rv->Init(std::move(platform_handle), serialized_write_buffer,
           serialized_write_buffer_size);
  return rv;
}

DataPipeProducerDispatcher::DataPipeProducerDispatcher(
    const MojoCreateDataPipeOptions& options)
    : options_(options), channel_(nullptr), error_(false), serialized_(false) {
}

DataPipeProducerDispatcher::~DataPipeProducerDispatcher() {
  // See comment in ~MessagePipeDispatcher.
  if (channel_ && internal::g_io_thread_task_runner->RunsTasksOnCurrentThread())
    channel_->Shutdown();
  else
    DCHECK(!channel_);
}

void DataPipeProducerDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  awakable_list_.CancelAll();
}

void DataPipeProducerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE, base::Bind(&DataPipeProducerDispatcher::CloseOnIO, this));
}

scoped_refptr<Dispatcher>
DataPipeProducerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  SerializeInternal();

  scoped_refptr<DataPipeProducerDispatcher> rv = Create(options_);
  serialized_write_buffer_.swap(rv->serialized_write_buffer_);
  rv->serialized_platform_handle_ = std::move(serialized_platform_handle_);
  rv->serialized_ = true;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeProducerDispatcher::WriteDataImplNoLock(
    const void* elements,
    uint32_t* num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();
  if (InTwoPhaseWrite())
    return MOJO_RESULT_BUSY;
  if (error_)
    return MOJO_RESULT_FAILED_PRECONDITION;
  if (*num_bytes % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  // For now, we ignore options.capacity_num_bytes as a total of all pending
  // writes (and just treat it per message). We will implement that later if
  // we need to. All current uses want all their data to be sent, and it's not
  // clear that this backpressure should be done at the mojo layer or at a
  // higher application layer.
  bool all_or_none = flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_write = all_or_none ? *num_bytes : 0;
  if (min_num_bytes_to_write > options_.capacity_num_bytes) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return MOJO_RESULT_OUT_OF_RANGE;
  }

  uint32_t num_bytes_to_write =
      std::min(*num_bytes, options_.capacity_num_bytes);
  if (num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  HandleSignalsState old_state = GetHandleSignalsStateImplNoLock();

  *num_bytes = num_bytes_to_write;
  WriteDataIntoMessages(elements, num_bytes_to_write);

  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (!new_state.equals(old_state))
    awakable_list_.AwakeForStateChange(new_state);
  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::BeginWriteDataImplNoLock(
    void** buffer,
    uint32_t* buffer_num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();
  if (InTwoPhaseWrite())
    return MOJO_RESULT_BUSY;
  if (error_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  // See comment in WriteDataImplNoLock about ignoring capacity_num_bytes.
  if (*buffer_num_bytes == 0)
    *buffer_num_bytes = options_.capacity_num_bytes;

  two_phase_data_.resize(*buffer_num_bytes);
  *buffer = &two_phase_data_[0];

  // TODO: if buffer_num_bytes.Get() > GetConfiguration().max_message_num_bytes
  // we can construct a MessageInTransit here. But then we need to make
  // MessageInTransit support changing its data size later.

  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::EndWriteDataImplNoLock(
    uint32_t num_bytes_written) {
  lock().AssertAcquired();
  if (!InTwoPhaseWrite())
    return MOJO_RESULT_FAILED_PRECONDITION;

  // Note: Allow successful completion of the two-phase write even if the other
  // side has been closed.
  MojoResult rv = MOJO_RESULT_OK;
  if (num_bytes_written > two_phase_data_.size() ||
      num_bytes_written % options_.element_num_bytes != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else if (channel_) {
    WriteDataIntoMessages(&two_phase_data_[0], num_bytes_written);
  }

  // Two-phase write ended even on failure.
  two_phase_data_.clear();
  // If we're now writable, we *became* writable (since we weren't writable
  // during the two-phase write), so awake producer awakables.
  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (new_state.satisfies(MOJO_HANDLE_SIGNAL_WRITABLE))
    awakable_list_.AwakeForStateChange(new_state);

  return rv;
}

HandleSignalsState DataPipeProducerDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();

  HandleSignalsState rv;
  if (!error_) {
    if (!InTwoPhaseWrite())
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  } else {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

MojoResult DataPipeProducerDispatcher::AddAwakableImplNoLock(
    Awakable* awakable,
    MojoHandleSignals signals,
    uintptr_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  if (channel_)
    channel_->EnsureLazyInitialized();
  HandleSignalsState state = GetHandleSignalsStateImplNoLock();
  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  awakable_list_.Add(awakable, signals, context);
  return MOJO_RESULT_OK;
}

void DataPipeProducerDispatcher::RemoveAwakableImplNoLock(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  awakable_list_.Remove(awakable);
  if (signals_state)
    *signals_state = GetHandleSignalsStateImplNoLock();
}

void DataPipeProducerDispatcher::StartSerializeImplNoLock(
    size_t* max_size,
    size_t* max_platform_handles) {
  if (!serialized_)
    SerializeInternal();

  DataPipe::StartSerialize(serialized_platform_handle_.is_valid(),
                           !serialized_write_buffer_.empty(), max_size,
                           max_platform_handles);
}

bool DataPipeProducerDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  ScopedPlatformHandle shared_memory_handle;
  size_t shared_memory_size = serialized_write_buffer_.size();
  if (shared_memory_size) {
    scoped_refptr<PlatformSharedBuffer> shared_buffer(
        internal::g_platform_support->CreateSharedBuffer(
            shared_memory_size));
    scoped_ptr<PlatformSharedBufferMapping> mapping(
        shared_buffer->Map(0, shared_memory_size));
    memcpy(mapping->GetBase(), &serialized_write_buffer_[0],
           shared_memory_size);
    shared_memory_handle.reset(shared_buffer->PassPlatformHandle().release());
  }

  DataPipe::EndSerialize(options_, std::move(serialized_platform_handle_),
                         std::move(shared_memory_handle), shared_memory_size,
                         destination, actual_size, platform_handles);
  CloseImplNoLock();
  return true;
}

void DataPipeProducerDispatcher::TransportStarted() {
  started_transport_.Acquire();
}

void DataPipeProducerDispatcher::TransportEnded() {
  started_transport_.Release();
}

bool DataPipeProducerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return InTwoPhaseWrite();
}

void DataPipeProducerDispatcher::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  CHECK(false) << "DataPipeProducerDispatcher shouldn't get any messages.";
}

void DataPipeProducerDispatcher::OnError(Error error) {
  switch (error) {
    case ERROR_READ_BROKEN:
    case ERROR_READ_BAD_MESSAGE:
    case ERROR_READ_UNKNOWN:
      LOG(ERROR) << "DataPipeProducerDispatcher shouldn't get read error.";
      break;
    case ERROR_READ_SHUTDOWN:
      // The other side was cleanly closed, so this isn't actually an error.
      DVLOG(1) << "DataPipeProducerDispatcher read error (shutdown)";
      break;
    case ERROR_WRITE:
      // Write errors are slightly notable: they probably shouldn't happen under
      // normal operation (but maybe the other side crashed).
      LOG(WARNING) << "DataPipeProducerDispatcher write error";
      break;
  }

  error_ = true;
  if (started_transport_.Try()) {
    base::AutoLock locker(lock());
    // We can get two OnError callbacks before the post task below completes.
    // Although RawChannel still has a pointer to this object until Shutdown is
    // called, that is safe since this class always does a PostTask to the IO
    // thread to self destruct.
    if (channel_) {
      awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
      channel_->Shutdown();
      channel_ = nullptr;
    }
    started_transport_.Release();
  } else {
    // We must be waiting to call ReleaseHandle. It will call Shutdown.
  }
}

bool DataPipeProducerDispatcher::InTwoPhaseWrite() const {
  return !two_phase_data_.empty();
}

bool DataPipeProducerDispatcher::WriteDataIntoMessages(
    const void* elements,
    uint32_t num_bytes) {
  // The maximum amount of data to send per message (make it a multiple of the
  // element size.
  size_t max_message_num_bytes = GetConfiguration().max_message_num_bytes;
  max_message_num_bytes -= max_message_num_bytes % options_.element_num_bytes;
  DCHECK_GT(max_message_num_bytes, 0u);

  uint32_t offset = 0;
  while (offset < num_bytes) {
    uint32_t message_num_bytes =
        std::min(static_cast<uint32_t>(max_message_num_bytes),
                 num_bytes - offset);
    scoped_ptr<MessageInTransit> message(new MessageInTransit(
        MessageInTransit::Type::MESSAGE, message_num_bytes,
        static_cast<const char*>(elements) + offset));
    if (!channel_->WriteMessage(std::move(message))) {
      error_ = true;
      return false;
    }

    offset += message_num_bytes;
  }

  return true;
}

void DataPipeProducerDispatcher::SerializeInternal() {
  // We need to stop watching handle immediately, even though not on IO thread,
  // so that other messages aren't read after this.
  if (channel_) {
    std::vector<char> serialized_read_buffer;
    std::vector<int> fds;
    bool write_error = false;
    serialized_platform_handle_ = channel_->ReleaseHandle(
        &serialized_read_buffer, &serialized_write_buffer_, &fds, &fds,
        &write_error);
    CHECK(serialized_read_buffer.empty());
    CHECK(fds.empty());
    if (write_error)
      serialized_platform_handle_.reset();
    channel_ = nullptr;
  }
  serialized_ = true;
}

}  // namespace edk
}  // namespace mojo
