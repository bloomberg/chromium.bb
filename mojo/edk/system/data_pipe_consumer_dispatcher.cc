// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/data_pipe.h"

namespace mojo {
namespace edk {

struct SharedMemoryHeader {
  uint32_t data_size;
  uint32_t read_buffer_size;
};

void DataPipeConsumerDispatcher::Init(
    ScopedPlatformHandle message_pipe,
    char* serialized_read_buffer, size_t serialized_read_buffer_size) {
  if (message_pipe.is_valid()) {
    channel_ = RawChannel::Create(std::move(message_pipe));
    channel_->SetSerializedData(
        serialized_read_buffer, serialized_read_buffer_size, nullptr, 0u,
        nullptr, nullptr);
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&DataPipeConsumerDispatcher::InitOnIO, this));
  } else {
    // The data pipe consumer could have read all the data and the producer
    // closed its end subsequently (before the consumer was sent). In that case
    // when we deserialize the consumer we must make sure to set error_ or
    // otherwise the peer-closed signal will never be satisfied.
    error_ = true;
  }
}

void DataPipeConsumerDispatcher::InitOnIO() {
  base::AutoLock locker(lock());
  calling_init_ = true;
  if (channel_)
    channel_->Init(this);
  calling_init_ = false;
}

void DataPipeConsumerDispatcher::CloseOnIO() {
  base::AutoLock locker(lock());
  if (channel_) {
    channel_->Shutdown();
    channel_ = nullptr;
  }
}

Dispatcher::Type DataPipeConsumerDispatcher::GetType() const {
  return Type::DATA_PIPE_CONSUMER;
}

scoped_refptr<DataPipeConsumerDispatcher>
DataPipeConsumerDispatcher::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  MojoCreateDataPipeOptions options;
  ScopedPlatformHandle shared_memory_handle;
  size_t shared_memory_size = 0;

  ScopedPlatformHandle platform_handle =
      DataPipe::Deserialize(source, size, platform_handles, &options,
      &shared_memory_handle, &shared_memory_size);

  scoped_refptr<DataPipeConsumerDispatcher> rv(Create(options));

  char* serialized_read_buffer = nullptr;
  size_t serialized_read_buffer_size = 0;
  scoped_refptr<PlatformSharedBuffer> shared_buffer;
  scoped_ptr<PlatformSharedBufferMapping> mapping;
  if (shared_memory_size) {
    shared_buffer = internal::g_platform_support->CreateSharedBufferFromHandle(
        shared_memory_size, std::move(shared_memory_handle));
    mapping = shared_buffer->Map(0, shared_memory_size);
    char* buffer = static_cast<char*>(mapping->GetBase());
    SharedMemoryHeader* header = reinterpret_cast<SharedMemoryHeader*>(buffer);
    buffer += sizeof(SharedMemoryHeader);
    if (header->data_size) {
      rv->data_.assign(buffer, buffer + header->data_size);
      buffer += header->data_size;
    }

    if (header->read_buffer_size) {
      serialized_read_buffer = buffer;
      serialized_read_buffer_size = header->read_buffer_size;
      buffer += header->read_buffer_size;
    }
  }

  rv->Init(std::move(platform_handle), serialized_read_buffer,
           serialized_read_buffer_size);
  return rv;
}

DataPipeConsumerDispatcher::DataPipeConsumerDispatcher(
    const MojoCreateDataPipeOptions& options)
    : options_(options),
      channel_(nullptr),
      calling_init_(false),
      in_two_phase_read_(false),
      two_phase_max_bytes_read_(0),
      error_(false),
      serialized_(false) {
}

DataPipeConsumerDispatcher::~DataPipeConsumerDispatcher() {
  // See comment in ~MessagePipeDispatcher.
  if (channel_ && internal::g_io_thread_task_runner->RunsTasksOnCurrentThread())
    channel_->Shutdown();
  else
    DCHECK(!channel_);
}

void DataPipeConsumerDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  awakable_list_.CancelAll();
}

void DataPipeConsumerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE, base::Bind(&DataPipeConsumerDispatcher::CloseOnIO, this));
}

scoped_refptr<Dispatcher>
DataPipeConsumerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  SerializeInternal();

  scoped_refptr<DataPipeConsumerDispatcher> rv = Create(options_);
  data_.swap(rv->data_);
  serialized_read_buffer_.swap(rv->serialized_read_buffer_);
  rv->serialized_platform_handle_ = std::move(serialized_platform_handle_);
  rv->serialized_ = true;

  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeConsumerDispatcher::ReadDataImplNoLock(
    void* elements,
    uint32_t* num_bytes,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();
  if (channel_)
    channel_->EnsureLazyInitialized();
  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  if ((flags & MOJO_READ_DATA_FLAG_QUERY)) {
    if ((flags & MOJO_READ_DATA_FLAG_PEEK) ||
        (flags & MOJO_READ_DATA_FLAG_DISCARD))
      return MOJO_RESULT_INVALID_ARGUMENT;
    DCHECK(!(flags & MOJO_READ_DATA_FLAG_DISCARD));  // Handled above.
    DVLOG_IF(2, elements)
        << "Query mode: ignoring non-null |elements|";
    *num_bytes = static_cast<uint32_t>(data_.size());
    return MOJO_RESULT_OK;
  }

  bool discard = false;
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD)) {
    // These flags are mutally exclusive.
    if (flags & MOJO_READ_DATA_FLAG_PEEK)
      return MOJO_RESULT_INVALID_ARGUMENT;
    DVLOG_IF(2, elements)
        << "Discard mode: ignoring non-null |elements|";
    discard = true;
  }

  uint32_t max_num_bytes_to_read = *num_bytes;
  if (max_num_bytes_to_read % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool all_or_none = flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_read =
      all_or_none ? max_num_bytes_to_read : 0;

  if (min_num_bytes_to_read > data_.size())
    return error_ ? MOJO_RESULT_FAILED_PRECONDITION : MOJO_RESULT_OUT_OF_RANGE;

  uint32_t bytes_to_read = std::min(max_num_bytes_to_read,
                                    static_cast<uint32_t>(data_.size()));
  if (bytes_to_read == 0)
    return error_ ? MOJO_RESULT_FAILED_PRECONDITION : MOJO_RESULT_SHOULD_WAIT;

  if (!discard)
    memcpy(elements, &data_[0], bytes_to_read);
  *num_bytes = bytes_to_read;

  bool peek = !!(flags & MOJO_READ_DATA_FLAG_PEEK);
  if (discard || !peek)
    data_.erase(data_.begin(), data_.begin() + bytes_to_read);

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::BeginReadDataImplNoLock(
    const void** buffer,
    uint32_t* buffer_num_bytes,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();
  if (channel_)
    channel_->EnsureLazyInitialized();
  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  // These flags may not be used in two-phase mode.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY) ||
      (flags & MOJO_READ_DATA_FLAG_PEEK))
    return MOJO_RESULT_INVALID_ARGUMENT;

  uint32_t max_num_bytes_to_read = static_cast<uint32_t>(data_.size());
  if (max_num_bytes_to_read == 0)
    return error_ ? MOJO_RESULT_FAILED_PRECONDITION : MOJO_RESULT_SHOULD_WAIT;

  in_two_phase_read_ = true;
  *buffer = &data_[0];
  *buffer_num_bytes = max_num_bytes_to_read;
  two_phase_max_bytes_read_ = max_num_bytes_to_read;

  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::EndReadDataImplNoLock(
    uint32_t num_bytes_read) {
  lock().AssertAcquired();
  if (!in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  HandleSignalsState old_state = GetHandleSignalsStateImplNoLock();
  MojoResult rv;
  if (num_bytes_read > two_phase_max_bytes_read_ ||
      num_bytes_read % options_.element_num_bytes != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else {
    rv = MOJO_RESULT_OK;
    data_.erase(data_.begin(), data_.begin() + num_bytes_read);
  }

  in_two_phase_read_ = false;
  two_phase_max_bytes_read_ = 0;
  if (!data_received_during_two_phase_read_.empty()) {
    if (data_.empty()) {
      data_received_during_two_phase_read_.swap(data_);
    } else {
      data_.insert(data_.end(), data_received_during_two_phase_read_.begin(),
                   data_received_during_two_phase_read_.end());
      data_received_during_two_phase_read_.clear();
    }
  }

  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (!new_state.equals(old_state))
    awakable_list_.AwakeForStateChange(new_state);

  return rv;
}

HandleSignalsState DataPipeConsumerDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();

  HandleSignalsState rv;
  if (!data_.empty()) {
    if (!in_two_phase_read_)
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  } else if (!error_) {
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  }

  if (error_)
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

MojoResult DataPipeConsumerDispatcher::AddAwakableImplNoLock(
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

void DataPipeConsumerDispatcher::RemoveAwakableImplNoLock(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  awakable_list_.Remove(awakable);
  if (signals_state)
    *signals_state = GetHandleSignalsStateImplNoLock();
}

void DataPipeConsumerDispatcher::StartSerializeImplNoLock(
    size_t* max_size,
    size_t* max_platform_handles) {
  if (!serialized_) {
    // Handles the case where we have messages read off RawChannel but not ready
    // by MojoReadMessage.
    SerializeInternal();
  }

  DataPipe::StartSerialize(serialized_platform_handle_.is_valid(),
                           !data_.empty() || !serialized_read_buffer_.empty(),
                           max_size, max_platform_handles);
}

bool DataPipeConsumerDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  ScopedPlatformHandle shared_memory_handle;
  size_t shared_memory_size =  data_.size() + serialized_read_buffer_.size();
  if (shared_memory_size) {
    shared_memory_size += sizeof(SharedMemoryHeader);
    SharedMemoryHeader header;
    header.data_size = static_cast<uint32_t>(data_.size());
    header.read_buffer_size =
        static_cast<uint32_t>(serialized_read_buffer_.size());

    scoped_refptr<PlatformSharedBuffer> shared_buffer(
        internal::g_platform_support->CreateSharedBuffer(
            shared_memory_size));
    scoped_ptr<PlatformSharedBufferMapping> mapping(
        shared_buffer->Map(0, shared_memory_size));

    char* start = static_cast<char*>(mapping->GetBase());
    memcpy(start, &header, sizeof(SharedMemoryHeader));
    start += sizeof(SharedMemoryHeader);

    if (!data_.empty()) {
      memcpy(start, &data_[0], data_.size());
      start += data_.size();
    }

    if (!serialized_read_buffer_.empty()) {
      memcpy(start, &serialized_read_buffer_[0],
             serialized_read_buffer_.size());
      start += serialized_read_buffer_.size();
    }

    shared_memory_handle.reset(shared_buffer->PassPlatformHandle().release());
  }

  DataPipe::EndSerialize(options_, std::move(serialized_platform_handle_),
                         std::move(shared_memory_handle), shared_memory_size,
                         destination, actual_size, platform_handles);
  CloseImplNoLock();
  return true;
}

void DataPipeConsumerDispatcher::TransportStarted() {
  started_transport_.Acquire();
}

void DataPipeConsumerDispatcher::TransportEnded() {
  started_transport_.Release();

  base::AutoLock locker(lock());

  // If transporting of DP failed, we might have got more data and didn't awake
  // for.
  // TODO(jam): should we care about only alerting if it was empty before
  // TransportStarted?
  if (!data_.empty())
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
}

bool DataPipeConsumerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return in_two_phase_read_;
}

void DataPipeConsumerDispatcher::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  const char* bytes_start = static_cast<const char*>(message_view.bytes());
  const char* bytes_end = bytes_start + message_view.num_bytes();
  if (started_transport_.Try()) {
    // We're not in the middle of being sent.

    // Can get synchronously called back in Init if there was initial data.
    scoped_ptr<base::AutoLock> locker;
    if (!calling_init_) {
      locker.reset(new base::AutoLock(lock()));
    }

    if (in_two_phase_read_) {
      data_received_during_two_phase_read_.insert(
          data_received_during_two_phase_read_.end(), bytes_start, bytes_end);
    } else {
      bool was_empty = data_.empty();
      data_.insert(data_.end(), bytes_start, bytes_end);
      if (was_empty)
        awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    }
    started_transport_.Release();
  } else {
    // See comment in MessagePipeDispatcher about why we can't and don't need
    // to lock here.
    data_.insert(data_.end(), bytes_start, bytes_end);
  }
}

void DataPipeConsumerDispatcher::OnError(Error error) {
  switch (error) {
    case ERROR_READ_SHUTDOWN:
      // The other side was cleanly closed, so this isn't actually an error.
      DVLOG(1) << "DataPipeConsumerDispatcher read error (shutdown)";
      break;
    case ERROR_READ_BROKEN:
      LOG(ERROR) << "DataPipeConsumerDispatcher read error (connection broken)";
      break;
    case ERROR_READ_BAD_MESSAGE:
      // Receiving a bad message means either a bug, data corruption, or
      // malicious attack (probably due to some other bug).
      LOG(ERROR) << "DataPipeConsumerDispatcher read error (received bad "
                 << "message)";
      break;
    case ERROR_READ_UNKNOWN:
      LOG(ERROR) << "DataPipeConsumerDispatcher read error (unknown)";
      break;
    case ERROR_WRITE:
      LOG(ERROR) << "DataPipeConsumerDispatcher shouldn't write messages";
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

void DataPipeConsumerDispatcher::SerializeInternal() {
  DCHECK(!in_two_phase_read_);
  // We need to stop watching handle immediately, even though not on IO thread,
  // so that other messages aren't read after this.
  if (channel_) {
    std::vector<char> serialized_write_buffer;
    std::vector<int> fds;
    bool write_error = false;
    serialized_platform_handle_ = channel_->ReleaseHandle(
        &serialized_read_buffer_, &serialized_write_buffer, &fds, &fds,
        &write_error);
    CHECK(serialized_write_buffer.empty());
    CHECK(fds.empty());
    CHECK(!write_error) << "DataPipeConsumerDispatcher doesn't write.";

    channel_ = nullptr;
  }

  serialized_ = true;
}

}  // namespace edk
}  // namespace mojo
