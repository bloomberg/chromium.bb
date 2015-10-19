// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_dispatcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/options_validation.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace edk {

// TODO(jam): do more tests on using channel on same thread if it supports it (
// i.e. with USE_CHROME_EDK and Windows). Also see ipc_channel_mojo.cc
bool g_use_channel_on_io_thread_only = true;

namespace {

const size_t kInvalidMessagePipeHandleIndex = static_cast<size_t>(-1);

struct MOJO_ALIGNAS(8) SerializedMessagePipeHandleDispatcher {
  // Could be |kInvalidMessagePipeHandleIndex| if the other endpoint of the MP
  // was closed.
  size_t platform_handle_index;
  bool write_error;

  size_t shared_memory_handle_index;  // (Or |kInvalidMessagePipeHandleIndex|.)
  uint32_t shared_memory_size;

  size_t serialized_read_buffer_size;
  size_t serialized_write_buffer_size;
  size_t serialized_messagage_queue_size;
};

char* SerializeBuffer(char* start, std::vector<char>* buffer) {
  if (buffer->size())
    memcpy(start, &(*buffer)[0], buffer->size());
  return start + buffer->size();
}

bool GetHandle(size_t index,
               PlatformHandleVector* platform_handles,
               ScopedPlatformHandle* handle) {
  if (index == kInvalidMessagePipeHandleIndex)
    return true;

  if (!platform_handles || index >= platform_handles->size()) {
    LOG(ERROR)
        << "Invalid serialized message pipe dispatcher (missing handles)";
    return false;
  }

  // We take ownership of the handle, so we have to invalidate the one in
  // |platform_handles|.
  handle->reset((*platform_handles)[index]);
  (*platform_handles)[index] = PlatformHandle();
  return true;
}

}  // namespace

// MessagePipeDispatcher -------------------------------------------------------

const MojoCreateMessagePipeOptions
    MessagePipeDispatcher::kDefaultCreateOptions = {
        static_cast<uint32_t>(sizeof(MojoCreateMessagePipeOptions)),
        MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE};

MojoResult MessagePipeDispatcher::ValidateCreateOptions(
    const MojoCreateMessagePipeOptions* in_options,
    MojoCreateMessagePipeOptions* out_options) {
  const MojoCreateMessagePipeOptionsFlags kKnownFlags =
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE;

  *out_options = kDefaultCreateOptions;
  if (!in_options)
    return MOJO_RESULT_OK;

  UserOptionsReader<MojoCreateMessagePipeOptions> reader(in_options);
  if (!reader.is_valid())
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (!OPTIONS_STRUCT_HAS_MEMBER(MojoCreateMessagePipeOptions, flags, reader))
    return MOJO_RESULT_OK;
  if ((reader.options().flags & ~kKnownFlags))
    return MOJO_RESULT_UNIMPLEMENTED;
  out_options->flags = reader.options().flags;

  // Checks for fields beyond |flags|:

  // (Nothing here yet.)

  return MOJO_RESULT_OK;
}

void MessagePipeDispatcher::Init(
    ScopedPlatformHandle message_pipe,
    char* serialized_read_buffer, size_t serialized_read_buffer_size,
    char* serialized_write_buffer, size_t serialized_write_buffer_size) {
  if (message_pipe.get().is_valid()) {
    channel_ = RawChannel::Create(message_pipe.Pass());

    // TODO(jam): It's probably cleaner to pass this in Init call.
    channel_->SetSerializedData(
        serialized_read_buffer, serialized_read_buffer_size,
        serialized_write_buffer, serialized_write_buffer_size);
    if (g_use_channel_on_io_thread_only) {
      internal::g_io_thread_task_runner->PostTask(
          FROM_HERE, base::Bind(&MessagePipeDispatcher::InitOnIO, this));
    } else {
      InitOnIO();
    }
    // TODO(jam): optimize for when running on IO thread?
  }
}

void MessagePipeDispatcher::InitOnIO() {
  base::AutoLock locker(lock());
  calling_init_ = true;
  if (channel_)
    channel_->Init(this);
  calling_init_ = false;
}

void MessagePipeDispatcher::CloseOnIO() {
  base::AutoLock locker(lock());

  if (channel_) {
    channel_->Shutdown();
    channel_ = nullptr;
  }
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return Type::MESSAGE_PIPE;
}

// TODO(jam): this is copied from RawChannelWin till I figure out what's the
// best way we want to share this. Need to also consider posix which does
// require access to the RawChannel.
// Since this is used for serialization of messages read/written to a MP that
// aren't consumed by Mojo primitives yet, there could be an unbounded number of
// them when a MP is being sent. As a result, even for POSIX we will probably
// want to send the handles to the shell process and exchange them for tokens
// (since we can be sure that the shell will respond to our IPCs, compared to
// the other end where we're sending the MP to, which may not be reading...).
ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
    size_t num_platform_handles,
    const void* platform_handle_table) {
  // TODO(jam): this code will have to be updated once it's used in a sandbox
  // and the receiving process doesn't have duplicate permission for the
  // receiver. Once there's a broker and we have a connection to it (possibly
  // through ConnectionManager), then we can make a sync IPC to it here to get a
  // token for this handle, and it will duplicate the handle to is process. Then
  // we pass the token to the receiver, which will then make a sync call to the
  // broker to get a duplicated handle. This will also allow us to avoid leaks
  // of the handle if the receiver dies, since the broker can notice that.
  DCHECK_GT(num_platform_handles, 0u);
  ScopedPlatformHandleVectorPtr rv(new PlatformHandleVector());

#if defined(OS_WIN)
  const char* serialization_data =
      static_cast<const char*>(platform_handle_table);
  for (size_t i = 0; i < num_platform_handles; i++) {
    DWORD pid = *reinterpret_cast<const DWORD*>(serialization_data);
    serialization_data += sizeof(DWORD);
    HANDLE source_handle = *reinterpret_cast<const HANDLE*>(serialization_data);
    serialization_data += sizeof(HANDLE);
    base::Process sender =
        base::Process::OpenWithAccess(pid, PROCESS_DUP_HANDLE);
    DCHECK(sender.IsValid());
    HANDLE target_handle = NULL;
    BOOL dup_result =
        DuplicateHandle(sender.Handle(), source_handle,
                        base::GetCurrentProcessHandle(), &target_handle, 0,
                        FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    DCHECK(dup_result);
    rv->push_back(PlatformHandle(target_handle));
  }
#else
  NOTREACHED() << "TODO(jam): implement";
#endif
  return rv.Pass();
}

scoped_refptr<MessagePipeDispatcher> MessagePipeDispatcher::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  if (size != sizeof(SerializedMessagePipeHandleDispatcher)) {
    LOG(ERROR) << "Invalid serialized message pipe dispatcher (bad size)";
    return nullptr;
  }

  const SerializedMessagePipeHandleDispatcher* serialization =
      static_cast<const SerializedMessagePipeHandleDispatcher*>(source);
  if (serialization->shared_memory_size !=
      (serialization->serialized_read_buffer_size +
       serialization->serialized_write_buffer_size +
       serialization->serialized_messagage_queue_size)) {
    LOG(ERROR) << "Invalid serialized message pipe dispatcher (bad struct)";
    return nullptr;
  }

  ScopedPlatformHandle platform_handle, shared_memory_handle;
  if (!GetHandle(serialization->platform_handle_index,
                 platform_handles, &platform_handle) ||
      !GetHandle(serialization->shared_memory_handle_index,
                 platform_handles, &shared_memory_handle)) {
    return nullptr;
  }

  char* serialized_read_buffer = nullptr;
  size_t serialized_read_buffer_size = 0;
  char* serialized_write_buffer = nullptr;
  size_t serialized_write_buffer_size = 0;
  char* message_queue_data = nullptr;
  size_t message_queue_size = 0;
  scoped_refptr<PlatformSharedBuffer> shared_buffer;
  scoped_ptr<PlatformSharedBufferMapping> mapping;
  if (shared_memory_handle.is_valid()) {
    shared_buffer = internal::g_platform_support->CreateSharedBufferFromHandle(
            serialization->shared_memory_size, shared_memory_handle.Pass());
    mapping = shared_buffer->Map(0, serialization->shared_memory_size);
    char* buffer = static_cast<char*>(mapping->GetBase());
    if (serialization->serialized_read_buffer_size) {
      serialized_read_buffer = buffer;
      serialized_read_buffer_size = serialization->serialized_read_buffer_size;
      buffer += serialized_read_buffer_size;
    }
    if (serialization->serialized_write_buffer_size) {
      serialized_write_buffer = buffer;
      serialized_write_buffer_size =
          serialization->serialized_write_buffer_size;
      buffer += serialized_write_buffer_size;
    }
    if (serialization->serialized_messagage_queue_size) {
      message_queue_data = buffer;
      message_queue_size = serialization->serialized_messagage_queue_size;
      buffer += message_queue_size;
    }
  }

  scoped_refptr<MessagePipeDispatcher> rv(
      Create(MessagePipeDispatcher::kDefaultCreateOptions));
  rv->Init(platform_handle.Pass(),
           serialized_read_buffer,
           serialized_read_buffer_size,
           serialized_write_buffer,
           serialized_write_buffer_size);
  rv->write_error_ = serialization->write_error;

  while (message_queue_size) {
    size_t message_size;
    CHECK(MessageInTransit::GetNextMessageSize(
              message_queue_data, message_queue_size, &message_size));
    MessageInTransit::View message_view(message_size, message_queue_data);
    message_queue_size -= message_size;
    message_queue_data += message_size;

    // TODO(jam): Copied below from RawChannelWin. See commment above
    // GetReadPlatformHandles.
    ScopedPlatformHandleVectorPtr temp_platform_handles;
    if (message_view.transport_data_buffer()) {
      size_t num_platform_handles;
      const void* platform_handle_table;
      TransportData::GetPlatformHandleTable(
          message_view.transport_data_buffer(), &num_platform_handles,
          &platform_handle_table);

      if (num_platform_handles > 0) {
        temp_platform_handles =
            GetReadPlatformHandles(num_platform_handles,
                                    platform_handle_table).Pass();
        if (!temp_platform_handles) {
          LOG(ERROR) << "Invalid number of platform handles received";
          return nullptr;
        }
      }
    }

    // TODO(jam): Copied below from RawChannelWin. See commment above
    // GetReadPlatformHandles.
    scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
    if (message_view.transport_data_buffer_size() > 0) {
      DCHECK(message_view.transport_data_buffer());
      message->SetDispatchers(TransportData::DeserializeDispatchers(
          message_view.transport_data_buffer(),
          message_view.transport_data_buffer_size(),
          temp_platform_handles.Pass()));
    }

    rv->message_queue_.AddMessage(message.Pass());
  }

  if (message_queue_size) {  // Should be empty by now.
    LOG(ERROR) << "Invalid queued messages";
    return nullptr;
  }

  return rv;
}

MessagePipeDispatcher::MessagePipeDispatcher()
    : channel_(nullptr),
      serialized_(false),
      calling_init_(false),
      write_error_(false) {
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the channel.
  DCHECK(!channel_);
}

void MessagePipeDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  awakable_list_.CancelAll();
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  if (g_use_channel_on_io_thread_only) {
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&MessagePipeDispatcher::CloseOnIO, this));
  } else {
    CloseOnIO();
  }
}

void MessagePipeDispatcher::SerializeInternal() {
  // We need to stop watching handle immediately, even though not on IO thread,
  // so that other messages aren't read after this.
  {
    if (channel_) {
      bool write_error = false;
      serialized_platform_handle_ = channel_->ReleaseHandle(
          &serialized_read_buffer_, &serialized_write_buffer_, &write_error);
      channel_ = nullptr;
      if (write_error)
        write_error = true;
    } else {
      // It's valid that the other side wrote some data and closed its end.
    }
  }

  DCHECK(serialized_message_queue_.empty());
  while (!message_queue_.IsEmpty()) {
    scoped_ptr<MessageInTransit> message = message_queue_.GetMessage();

    // When MojoWriteMessage is called, the MessageInTransit doesn't have
    // dispatchers set and CreateEquivaent... is called since the dispatchers
    // can be referenced by others. here dispatchers aren't referenced by
    // others, but rawchannel can still call to them. so since we dont call
    // createequiv, manually call TransportStarted and TransportEnd.
    DispatcherVector dispatchers;
    if (message->has_dispatchers())
      dispatchers = *message->dispatchers();
    for (size_t i = 0; i < dispatchers.size(); ++i)
      dispatchers[i]->TransportStarted();

    // TODO(jam): this handling for dispatchers only works on windows where we
    // send transportdata as bytes instead of as parameters to sendmsg.
    message->SerializeAndCloseDispatchers();
    // cont'd below

    size_t main_buffer_size = message->main_buffer_size();
    size_t transport_data_buffer_size = message->transport_data() ?
        message->transport_data()->buffer_size() : 0;

    serialized_message_queue_.insert(
        serialized_message_queue_.end(),
        static_cast<const char*>(message->main_buffer()),
        static_cast<const char*>(message->main_buffer()) + main_buffer_size);

    // cont'd
    if (transport_data_buffer_size != 0) {
#if defined(OS_WIN)
      // TODO(jam): copied from RawChannelWin::WriteNoLock(
      if (RawChannel::GetSerializedPlatformHandleSize()) {
        char* serialization_data =
            static_cast<char*>(message->transport_data()->buffer()) +
             message->transport_data()->platform_handle_table_offset();
        PlatformHandleVector* all_platform_handles =
            message->transport_data()->platform_handles();
        if (all_platform_handles) {
          DWORD current_process_id = base::GetCurrentProcId();
          for (size_t i = 0; i < all_platform_handles->size(); i++) {
            *reinterpret_cast<DWORD*>(serialization_data) = current_process_id;
            serialization_data += sizeof(DWORD);
            *reinterpret_cast<HANDLE*>(serialization_data) =
                all_platform_handles->at(i).handle;
            serialization_data += sizeof(HANDLE);
            all_platform_handles->at(i) = PlatformHandle();
          }
        }
      }

      serialized_message_queue_.insert(
          serialized_message_queue_.end(),
          static_cast<const char*>(message->transport_data()->buffer()),
          static_cast<const char*>(message->transport_data()->buffer()) +
              transport_data_buffer_size);
#else
      NOTREACHED() << "TODO(jam) implement";
#endif
    }

    for (size_t i = 0; i < dispatchers.size(); ++i)
      dispatchers[i]->TransportEnded();
  }

  serialized_ = true;
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  SerializeInternal();

  // TODO(vtl): Currently, there are no options, so we just use
  // |kDefaultCreateOptions|. Eventually, we'll have to duplicate the options
  // too.
  scoped_refptr<MessagePipeDispatcher> rv = Create(kDefaultCreateOptions);
  rv->serialized_platform_handle_ = serialized_platform_handle_.Pass();
  serialized_message_queue_.swap(rv->serialized_message_queue_);
  serialized_read_buffer_.swap(rv->serialized_read_buffer_);
  serialized_write_buffer_.swap(rv->serialized_write_buffer_);
  rv->serialized_ = true;
  rv->write_error_ = write_error_;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {

  DCHECK(!transports ||
         (transports->size() > 0 &&
          transports->size() <= GetConfiguration().max_message_num_handles));

  lock().AssertAcquired();

  if (!channel_ || write_error_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  if (num_bytes > GetConfiguration().max_message_num_bytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::Type::MESSAGE, num_bytes, bytes));
  if (transports) {
    MojoResult result = AttachTransportsNoLock(message.get(), transports);
    if (result != MOJO_RESULT_OK)
      return result;
  }

  message->SerializeAndCloseDispatchers();
  channel_->WriteMessage(message.Pass());

  return MOJO_RESULT_OK;
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes,
    uint32_t* num_bytes,
    DispatcherVector* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();
  if (channel_)
    channel_->EnsureLazyInitialized();
  DCHECK(!dispatchers || dispatchers->empty());

  const uint32_t max_bytes = !num_bytes ? 0 : *num_bytes;
  const uint32_t max_num_dispatchers = num_dispatchers ? *num_dispatchers : 0;

  if (message_queue_.IsEmpty())
    return channel_ ? MOJO_RESULT_SHOULD_WAIT : MOJO_RESULT_FAILED_PRECONDITION;

  // TODO(vtl): If |flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD|, we could pop
  // and release the lock immediately.
  bool enough_space = true;
  MessageInTransit* message = message_queue_.PeekMessage();
  if (num_bytes)
    *num_bytes = message->num_bytes();
  if (message->num_bytes() <= max_bytes)
    memcpy(bytes, message->bytes(), message->num_bytes());
  else
    enough_space = false;

  if (DispatcherVector* queued_dispatchers = message->dispatchers()) {
    if (num_dispatchers)
      *num_dispatchers = static_cast<uint32_t>(queued_dispatchers->size());
    if (enough_space) {
      if (queued_dispatchers->empty()) {
        // Nothing to do.
      } else if (queued_dispatchers->size() <= max_num_dispatchers) {
        DCHECK(dispatchers);
        dispatchers->swap(*queued_dispatchers);
      } else {
        enough_space = false;
      }
    }
  } else {
    if (num_dispatchers)
      *num_dispatchers = 0;
  }

  message = nullptr;

  if (enough_space || (flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)) {
    message_queue_.DiscardMessage();

    // Now it's empty, thus no longer readable.
    if (message_queue_.IsEmpty()) {
      // It's currently not possible to wait for non-readability, but we should
      // do the state change anyway.
      awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    }
  }

  if (!enough_space)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return MOJO_RESULT_OK;
}

HandleSignalsState MessagePipeDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();

  HandleSignalsState rv;
  if (!message_queue_.IsEmpty())
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (channel_ || !message_queue_.IsEmpty())
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (channel_ && !write_error_) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  }
  if (!channel_ || write_error_)
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

MojoResult MessagePipeDispatcher::AddAwakableImplNoLock(
    Awakable* awakable,
    MojoHandleSignals signals,
    uint32_t context,
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

void MessagePipeDispatcher::RemoveAwakableImplNoLock(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();

  awakable_list_.Remove(awakable);
  if (signals_state)
    *signals_state = GetHandleSignalsStateImplNoLock();
}

void MessagePipeDispatcher::StartSerializeImplNoLock(
    size_t* max_size,
    size_t* max_platform_handles) {
  if (!serialized_)
    SerializeInternal();

  *max_platform_handles = 0;
  if (serialized_platform_handle_.is_valid())
    (*max_platform_handles)++;
  if (!serialized_read_buffer_.empty() ||
      !serialized_write_buffer_.empty() ||
      !serialized_message_queue_.empty())
    (*max_platform_handles)++;
  *max_size = sizeof(SerializedMessagePipeHandleDispatcher);
}

bool MessagePipeDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  CloseImplNoLock();
  SerializedMessagePipeHandleDispatcher* serialization =
      static_cast<SerializedMessagePipeHandleDispatcher*>(destination);
  if (serialized_platform_handle_.is_valid()) {
    serialization->platform_handle_index = platform_handles->size();
    platform_handles->push_back(serialized_platform_handle_.release());
  } else {
    serialization->platform_handle_index = kInvalidMessagePipeHandleIndex;
  }

  serialization->write_error = write_error_;
  serialization->serialized_read_buffer_size = serialized_read_buffer_.size();
  serialization->serialized_write_buffer_size = serialized_write_buffer_.size();
  serialization->serialized_messagage_queue_size =
      serialized_message_queue_.size();

  serialization->shared_memory_size = static_cast<uint32_t>(
      serialization->serialized_read_buffer_size +
      serialization->serialized_write_buffer_size +
      serialization->serialized_messagage_queue_size);
  if (serialization->shared_memory_size) {
    scoped_refptr<PlatformSharedBuffer> shared_buffer(
        internal::g_platform_support->CreateSharedBuffer(
            serialization->shared_memory_size));
    scoped_ptr<PlatformSharedBufferMapping> mapping(
        shared_buffer->Map(0, serialization->shared_memory_size));
    char* start = static_cast<char*>(mapping->GetBase());
    start = SerializeBuffer(start, &serialized_read_buffer_);
    start = SerializeBuffer(start, &serialized_write_buffer_);
    start = SerializeBuffer(start, &serialized_message_queue_);

    serialization->shared_memory_handle_index = platform_handles->size();
    platform_handles->push_back(shared_buffer->PassPlatformHandle().release());
  } else {
    serialization->shared_memory_handle_index = kInvalidMessagePipeHandleIndex;
  }

  *actual_size = sizeof(SerializedMessagePipeHandleDispatcher);
  return true;
}

void MessagePipeDispatcher::TransportStarted() {
  started_transport_.Acquire();
}

void MessagePipeDispatcher::TransportEnded() {
  started_transport_.Release();

  base::AutoLock locker(lock());

  // If transporting of MPD failed, we might have got more data and didn't
  // awake for.
  // TODO(jam): should we care about only alerting if it was empty before
  // TransportStarted?
  if (!message_queue_.IsEmpty())
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
}

void MessagePipeDispatcher::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
  if (message_view.transport_data_buffer_size() > 0) {
    DCHECK(message_view.transport_data_buffer());
    message->SetDispatchers(TransportData::DeserializeDispatchers(
        message_view.transport_data_buffer(),
        message_view.transport_data_buffer_size(), platform_handles.Pass()));
  }

  if (started_transport_.Try()) {
    // we're not in the middle of being sent

    // Can get synchronously called back in Init if there was initial data.
    scoped_ptr<base::AutoLock> locker;
    if (!calling_init_) {
      locker.reset(new base::AutoLock(lock()));
    }

    bool was_empty = message_queue_.IsEmpty();
    message_queue_.AddMessage(message.Pass());
    if (was_empty)
      awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());

    started_transport_.Release();
  } else {
    // If RawChannel is calling OnRead, that means it has its read_lock_
    // acquired. That means StartSerialize can't be accessing message queue as
    // it waits on ReleaseHandle first which acquires readlock_.
    message_queue_.AddMessage(message.Pass());
  }
}

void MessagePipeDispatcher::OnError(Error error) {
  // If there's a read error, then the other side of the pipe is closed. By
  // definition, we can't write since there's no one to read it. And we can't
  // read anymore, since we just got a read erorr. So we close the pipe.
  // If there's a write error, then we stop writing. But we keep the pipe open
  // until we finish reading everything in it. This is because it's valid for
  // one endpoint to write some data and close their pipe immediately. Even
  // though the other end can't write anymore, it should still get all the data.
  switch (error) {
    case ERROR_READ_SHUTDOWN:
      // The other side was cleanly closed, so this isn't actually an error.
      DVLOG(1) << "MessagePipeDispatcher read error (shutdown)";
      break;
    case ERROR_READ_BROKEN:
      LOG(ERROR) << "MessagePipeDispatcher read error (connection broken)";
      break;
    case ERROR_READ_BAD_MESSAGE:
      // Receiving a bad message means either a bug, data corruption, or
      // malicious attack (probably due to some other bug).
      LOG(ERROR) << "MessagePipeDispatcher read error (received bad message)";
      break;
    case ERROR_READ_UNKNOWN:
      LOG(ERROR) << "MessagePipeDispatcher read error (unknown)";
      break;
    case ERROR_WRITE:
      // Write errors are slightly notable: they probably shouldn't happen under
      // normal operation (but maybe the other side crashed).
      LOG(WARNING) << "MessagePipeDispatcher write error";
      DCHECK_EQ(write_error_, false) << "Should only get one write error.";
      write_error_ = true;
      break;
  }

  if (started_transport_.Try()) {
    base::AutoLock locker(lock());
    // We can get two OnError callbacks before the post task below completes.
    // Although RawChannel still has a pointer to this object until Shutdown is
    // called, that is safe since this class always does a PostTask to the IO
    // thread to self destruct.
    if (channel_ && error != ERROR_WRITE) {
      channel_->Shutdown();
      channel_ = nullptr;
    }
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    started_transport_.Release();
  } else {
    // We must be waiting to call ReleaseHandle. It will call Shutdown.
  }
}

MojoResult MessagePipeDispatcher::AttachTransportsNoLock(
    MessageInTransit* message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(!message->has_dispatchers());

  // You're not allowed to send either handle to a message pipe over the message
  // pipe, so check for this. (The case of trying to write a handle to itself is
  // taken care of by |Core|. That case kind of makes sense, but leads to
  // complications if, e.g., both sides try to do the same thing with their
  // respective handles simultaneously. The other case, of trying to write the
  // peer handle to a handle, doesn't make sense -- since no handle will be
  // available to read the message from.)
  for (size_t i = 0; i < transports->size(); i++) {
    if (!(*transports)[i].is_valid())
      continue;
    if ((*transports)[i].GetType() == Dispatcher::Type::MESSAGE_PIPE) {
      MessagePipeDispatcher* mp =
          static_cast<MessagePipeDispatcher*>(((*transports)[i]).dispatcher());
      if (channel_ && mp->channel_ && channel_->IsOtherEndOf(mp->channel_)) {
        // The other case should have been disallowed by |Core|. (Note: |port|
        // is the peer port of the handle given to |WriteMessage()|.)
        return MOJO_RESULT_INVALID_ARGUMENT;
      }
    }
  }

  // Clone the dispatchers and attach them to the message. (This must be done as
  // a separate loop, since we want to leave the dispatchers alone on failure.)
  scoped_ptr<DispatcherVector> dispatchers(new DispatcherVector());
  dispatchers->reserve(transports->size());
  for (size_t i = 0; i < transports->size(); i++) {
    if ((*transports)[i].is_valid()) {
      dispatchers->push_back(
          (*transports)[i].CreateEquivalentDispatcherAndClose());
    } else {
      LOG(WARNING) << "Enqueueing null dispatcher";
      dispatchers->push_back(nullptr);
    }
  }
  message->SetDispatchers(dispatchers.Pass());
  return MOJO_RESULT_OK;
}

}  // namespace edk
}  // namespace mojo
