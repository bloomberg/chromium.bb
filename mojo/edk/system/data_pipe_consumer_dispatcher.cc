// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_consumer_dispatcher.h"

#include <algorithm>

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

void DataPipeConsumerDispatcher::Init(ScopedPlatformHandle message_pipe) {
  if (message_pipe.is_valid()) {
    channel_ = RawChannel::Create(message_pipe.Pass());
    if (!serialized_read_buffer_.empty())
      channel_->SetInitialReadBufferData(
          &serialized_read_buffer_[0], serialized_read_buffer_.size());
    serialized_read_buffer_.clear();
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&DataPipeConsumerDispatcher::InitOnIO, this));
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

  if (shared_memory_size) {
    scoped_refptr<PlatformSharedBuffer> shared_buffer(
        internal::g_platform_support->CreateSharedBufferFromHandle(
            shared_memory_size, shared_memory_handle.Pass()));;
    scoped_ptr<PlatformSharedBufferMapping> mapping(
        shared_buffer->Map(0, shared_memory_size));
    char* buffer = static_cast<char*>(mapping->GetBase());
    SharedMemoryHeader* header = reinterpret_cast<SharedMemoryHeader*>(buffer);
    buffer += sizeof(SharedMemoryHeader);
    if (header->data_size) {
      rv->data_.resize(header->data_size);
      memcpy(&rv->data_[0], buffer, header->data_size);
      buffer += header->data_size;
    }
    if (header->read_buffer_size) {
      rv->serialized_read_buffer_.resize(header->read_buffer_size);
      memcpy(&rv->serialized_read_buffer_[0], buffer, header->read_buffer_size);
      buffer += header->read_buffer_size;
    }

  }

  if (platform_handle.is_valid())
    rv->Init(platform_handle.Pass());
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
  // |Close()|/|CloseImplNoLock()| should have taken care of the channel.
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
  rv->channel_ = channel_;
  channel_ = nullptr;
  rv->options_ = options_;
  data_.swap(rv->data_);
  serialized_read_buffer_.swap(rv->serialized_read_buffer_);
  rv->serialized_platform_handle_ = serialized_platform_handle_.Pass();
  rv->serialized_ = true;

  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeConsumerDispatcher::ReadDataImplNoLock(
    void* elements,
    uint32_t* num_bytes,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();
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
  if (in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  // These flags may not be used in two-phase mode.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY) ||
      (flags & MOJO_READ_DATA_FLAG_PEEK))
    return MOJO_RESULT_INVALID_ARGUMENT;

  bool all_or_none = flags & MOJO_READ_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_read = 0;
  if (all_or_none) {
    min_num_bytes_to_read = *buffer_num_bytes;
    if (min_num_bytes_to_read % options_.element_num_bytes != 0)
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  uint32_t max_num_bytes_to_read = static_cast<uint32_t>(data_.size());
  if (min_num_bytes_to_read > max_num_bytes_to_read)
    return error_ ? MOJO_RESULT_FAILED_PRECONDITION : MOJO_RESULT_OUT_OF_RANGE;
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

  // If we're now readable, we *became* readable (since we weren't readable
  // during the two-phase read), so awake consumer awakables.
  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (new_state.satisfies(MOJO_HANDLE_SIGNAL_READABLE))
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
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
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
                           !data_.empty(),
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

  DataPipe::EndSerialize(
      options_,
      serialized_platform_handle_.Pass(),
      shared_memory_handle.Pass(),
      shared_memory_size, destination, actual_size,
      platform_handles);
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
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));

  if (started_transport_.Try()) {
    // We're not in the middle of being sent.

    // Can get synchronously called back in Init if there was initial data.
    scoped_ptr<base::AutoLock> locker;
    if (!calling_init_) {
      locker.reset(new base::AutoLock(lock()));
    }

    size_t old_size = data_.size();
    data_.resize(old_size + message->num_bytes());
    memcpy(&data_[old_size], message->bytes(), message->num_bytes());
    if (!old_size)
      awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    started_transport_.Release();
  } else {
    size_t old_size = data_.size();
    data_.resize(old_size + message->num_bytes());
    memcpy(&data_[old_size], message->bytes(), message->num_bytes());
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
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    started_transport_.Release();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::Shutdown, base::Unretained(channel_)));
    channel_ = nullptr;
  } else {
    // We must be waiting to call ReleaseHandle. It will call Shutdown.
  }
}

void DataPipeConsumerDispatcher::SerializeInternal() {
  // need to stop watching handle immediately, even tho not on IO thread, so
  // that other messages aren't read after this.
  if (channel_) {
    serialized_platform_handle_ =
        channel_->ReleaseHandle(&serialized_read_buffer_);

    channel_ = nullptr;
    serialized_ = true;
  }
}

}  // namespace edk
}  // namespace mojo
