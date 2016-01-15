// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/message_pipe_dispatcher.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_handle_utils.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/broker.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/options_validation.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace edk {

namespace {

const uint32_t kInvalidMessagePipeHandleIndex = static_cast<uint32_t>(-1);

struct MOJO_ALIGNAS(8) SerializedMessagePipeHandleDispatcher {
  MOJO_ALIGNAS(1) bool transferable;
  MOJO_ALIGNAS(1) bool write_error;
  MOJO_ALIGNAS(8) uint64_t pipe_id;  // If transferable is false.
  // The following members are only set if transferable is true.
  // Could be |kInvalidMessagePipeHandleIndex| if the other endpoint of the MP
  // was closed.
  MOJO_ALIGNAS(4) uint32_t platform_handle_index;

  MOJO_ALIGNAS(4) uint32_t
      shared_memory_handle_index;  // (Or |kInvalidMessagePipeHandleIndex|.)
  MOJO_ALIGNAS(4) uint32_t shared_memory_size;

  MOJO_ALIGNAS(4) uint32_t serialized_read_buffer_size;
  MOJO_ALIGNAS(4) uint32_t serialized_write_buffer_size;
  MOJO_ALIGNAS(4) uint32_t serialized_message_queue_size;

  // These are the FDs required as part of serializing channel_ and
  // message_queue_. This is only used on POSIX.
  MOJO_ALIGNAS(4)
  uint32_t serialized_fds_index;  // (Or |kInvalidMessagePipeHandleIndex|.)
  MOJO_ALIGNAS(4) uint32_t serialized_read_fds_length;
  MOJO_ALIGNAS(4) uint32_t serialized_write_fds_length;
  MOJO_ALIGNAS(4) uint32_t serialized_message_fds_length;
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

#if defined(OS_POSIX)
void ClosePlatformHandles(std::vector<int>* fds) {
  for (size_t i = 0; i < fds->size(); ++i)
    PlatformHandle((*fds)[i]).CloseIfNecessary();
}
#endif

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
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_NONE |
      MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE;

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
    char* serialized_write_buffer, size_t serialized_write_buffer_size,
    std::vector<int>* serialized_read_fds,
    std::vector<int>* serialized_write_fds) {
  CHECK(transferable_);
  if (message_pipe.get().is_valid()) {
    channel_ = RawChannel::Create(std::move(message_pipe));

    // TODO(jam): It's probably cleaner to pass this in Init call.
    channel_->SetSerializedData(
        serialized_read_buffer, serialized_read_buffer_size,
        serialized_write_buffer, serialized_write_buffer_size,
        serialized_read_fds, serialized_write_fds);
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&MessagePipeDispatcher::InitOnIO, this));
  }
}

void MessagePipeDispatcher::InitNonTransferable(uint64_t pipe_id) {
  CHECK(!transferable_);
  pipe_id_ = pipe_id;
}

void MessagePipeDispatcher::InitOnIO() {
  base::AutoLock locker(lock());
  CHECK(transferable_);
  calling_init_ = true;
  if (channel_)
    channel_->Init(this);
  calling_init_ = false;
}

void MessagePipeDispatcher::CloseOnIOAndRelease() {
  {
    base::AutoLock locker(lock());
    CloseOnIO();
  }
  Release();  // To match CloseImplNoLock.
}

void MessagePipeDispatcher::CloseOnIO() {
  lock().AssertAcquired();

  if (channel_) {
    // If we closed the channel now then in-flight message pipes wouldn't get
    // closed, and their other side wouldn't get a connection error notification
    // which could lead to hangs or leaks. So we ask the other side of this
    // message pipe to close, which ensures that we have dispatched all
    // in-flight message pipes.
    DCHECK(!close_requested_);
    close_requested_ = true;
    AddRef();
    scoped_ptr<MessageInTransit> message(new MessageInTransit(
        MessageInTransit::Type::QUIT_MESSAGE, 0, nullptr));
    if (!transferable_)
      message->set_route_id(pipe_id_);
    channel_->WriteMessage(std::move(message));
    return;
  }

  if (!transferable_ &&
      (non_transferable_state_ == CONNECT_CALLED ||
       non_transferable_state_ == WAITING_FOR_READ_OR_WRITE)) {
    if (non_transferable_state_ == WAITING_FOR_READ_OR_WRITE)
      RequestNontransferableChannel();

    // We can't cancel the pending request yet, since the other side of the
    // message pipe would want to get pending outgoing messages (if any) or at
    // least know that this end was closed. So keep this object alive until
    // then.
    non_transferable_state_ = WAITING_FOR_CONNECT_TO_CLOSE;
    AddRef();
  }
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return Type::MESSAGE_PIPE;
}

void MessagePipeDispatcher::GotNonTransferableChannel(RawChannel* channel) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock locker(lock());
  channel_ = channel;
  while (!non_transferable_outgoing_message_queue_.IsEmpty()) {
    channel_->WriteMessage(
        non_transferable_outgoing_message_queue_.GetMessage());
  }

  if (non_transferable_state_ == WAITING_FOR_CONNECT_TO_CLOSE) {
    non_transferable_state_ = CONNECTED;
    // We kept this object alive until it's connected, we can close it now.
    CloseOnIO();
    // Balance the AddRef in CloseOnIO.
    Release();
    return;
  }

  non_transferable_state_ = CONNECTED;
}

#if defined(OS_WIN)
// TODO(jam): this is copied from RawChannelWin till I figure out what's the
// best way we want to share this.
// Since this is used for serialization of messages read/written to a MP that
// aren't consumed by Mojo primitives yet, there could be an unbounded number of
// them when a MP is being sent. As a result, even for POSIX we will probably
// want to send the handles to the shell process and exchange them for tokens
// (since we can be sure that the shell will respond to our IPCs, compared to
// the other end where we're sending the MP to, which may not be reading...).
ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
    size_t num_platform_handles,
    const void* platform_handle_table) {
  ScopedPlatformHandleVectorPtr rv(new PlatformHandleVector());
  rv->resize(num_platform_handles);

  const uint64_t* tokens =
      static_cast<const uint64_t*>(platform_handle_table);
  internal::g_broker->TokenToHandle(tokens, num_platform_handles, &rv->at(0));
  return rv.Pass();
}
#endif

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

  scoped_refptr<MessagePipeDispatcher> rv(
      new MessagePipeDispatcher(serialization->transferable));
  if (!rv->transferable_) {
    rv->InitNonTransferable(serialization->pipe_id);
    return rv;
  }

  if (serialization->shared_memory_size !=
      (serialization->serialized_read_buffer_size +
       serialization->serialized_write_buffer_size +
       serialization->serialized_message_queue_size)) {
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
        serialization->shared_memory_size, std::move(shared_memory_handle));
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
    if (serialization->serialized_message_queue_size) {
      message_queue_data = buffer;
      message_queue_size = serialization->serialized_message_queue_size;
      buffer += message_queue_size;
    }
  }

  rv->write_error_ = serialization->write_error;

  std::vector<int> serialized_read_fds;
  std::vector<int> serialized_write_fds;
#if defined(OS_POSIX)
  std::vector<int> serialized_fds;
  size_t serialized_fds_index = 0;

  size_t total_fd_count = serialization->serialized_read_fds_length +
                          serialization->serialized_write_fds_length +
                          serialization->serialized_message_fds_length;
  for (size_t i = 0; i < total_fd_count; ++i) {
    ScopedPlatformHandle handle;
    if (!GetHandle(serialization->serialized_fds_index + i, platform_handles,
        &handle)) {
      ClosePlatformHandles(&serialized_fds);
      return nullptr;
    }
    serialized_fds.push_back(handle.release().handle);
  }

  serialized_read_fds.assign(
      serialized_fds.begin(),
      serialized_fds.begin() + serialization->serialized_read_fds_length);
  serialized_fds_index += serialization->serialized_read_fds_length;
  serialized_write_fds.assign(
      serialized_fds.begin() + serialized_fds_index,
      serialized_fds.begin() + serialized_fds_index +
          serialization->serialized_write_fds_length);
  serialized_fds_index += serialization->serialized_write_fds_length;
#endif

  while (message_queue_size) {
    size_t message_size;
    if (!MessageInTransit::GetNextMessageSize(
            message_queue_data, message_queue_size, &message_size)) {
      NOTREACHED() << "Couldn't read message size from serialized data.";
      return nullptr;
    }
    if (message_size > message_queue_size) {
      NOTREACHED() << "Invalid serialized message size.";
      return nullptr;
    }
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
#if defined(OS_WIN)
        temp_platform_handles =
            GetReadPlatformHandles(num_platform_handles,
                                    platform_handle_table).Pass();
        if (!temp_platform_handles) {
          LOG(ERROR) << "Invalid number of platform handles received";
          return nullptr;
        }
#else
        temp_platform_handles.reset(new PlatformHandleVector());
        for (size_t i = 0; i < num_platform_handles; ++i)
          temp_platform_handles->push_back(
              PlatformHandle(serialized_fds[serialized_fds_index++]));
#endif
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
          std::move(temp_platform_handles)));
    }

    rv->message_queue_.AddMessage(std::move(message));
  }

  rv->Init(std::move(platform_handle), serialized_read_buffer,
           serialized_read_buffer_size, serialized_write_buffer,
           serialized_write_buffer_size, &serialized_read_fds,
           &serialized_write_fds);

  if (message_queue_size) {  // Should be empty by now.
    LOG(ERROR) << "Invalid queued messages";
    return nullptr;
  }

  return rv;
}

MessagePipeDispatcher::MessagePipeDispatcher(bool transferable)
    : channel_(nullptr),
      serialized_read_fds_length_(0u),
      serialized_write_fds_length_(0u),
      serialized_message_fds_length_(0u),
      pipe_id_(0),
      non_transferable_state_(WAITING_FOR_READ_OR_WRITE),
      serialized_(false),
      calling_init_(false),
      write_error_(false),
      transferable_(transferable),
      close_requested_(false) {
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the channel. The
  // exception is if they posted a task to run CloseOnIO but the IO thread shut
  // down and so when it was deleting pending tasks it caused the last reference
  // to destruct this object. In that case, safe to destroy the channel.
  if (channel_ &&
      internal::g_io_thread_task_runner->RunsTasksOnCurrentThread()) {
    if (transferable_) {
      channel_->Shutdown();
    } else {
      internal::g_broker->CloseMessagePipe(pipe_id_, this);
    }
  } else {
    DCHECK(!channel_);
  }
#if defined(OS_POSIX)
  ClosePlatformHandles(&serialized_fds_);
#endif
}

void MessagePipeDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  awakable_list_.CancelAll();
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  // This early circuit fixes leak in unit tests. There's nothing to do in the
  // posted task.
  if (!transferable_ && non_transferable_state_ == CLOSED)
    return;

  // We take a manual refcount because at shutdown, the task below might not get
  // a chance to execute. If that happens, the RawChannel will still call our
  // OnError method because it always runs (since it watches thread
  // destruction). So to avoid UAF, manually add a reference and only release it
  // if the task runs.
  AddRef();
  if (!internal::g_io_thread_task_runner->PostTask(
          FROM_HERE,
          base::Bind(&MessagePipeDispatcher::CloseOnIOAndRelease, this))) {
    // Avoid a shutdown leak in unittests. If the thread is shutting down,
    // we can't connect to the other end to let it know that we're closed either
    // way.
    if (!transferable_ && non_transferable_state_ == WAITING_FOR_READ_OR_WRITE)
      Release();
  }
}

void MessagePipeDispatcher::SerializeInternal() {
  serialized_ = true;
  if (!transferable_) {
    CHECK(non_transferable_state_ == WAITING_FOR_READ_OR_WRITE)
        << "Non transferable message pipe being sent after read/write/waited. "
        << "MOJO_CREATE_MESSAGE_PIPE_OPTIONS_FLAG_TRANSFERABLE must be used if "
        << "the pipe can be sent after it's read or written. This message pipe "
        << "was previously bound at:\n"
        << non_transferable_bound_stack_->ToString();

    non_transferable_state_ = SERIALISED;
    return;
  }

  // We need to stop watching handle immediately, even though not on IO thread,
  // so that other messages aren't read after this.
  std::vector<int> serialized_read_fds, serialized_write_fds;
  if (channel_) {
    bool write_error = false;

    serialized_platform_handle_ = channel_->ReleaseHandle(
        &serialized_read_buffer_, &serialized_write_buffer_,
        &serialized_read_fds, &serialized_write_fds, &write_error);
    serialized_fds_.insert(serialized_fds_.end(), serialized_read_fds.begin(),
                          serialized_read_fds.end());
    serialized_read_fds_length_ = serialized_read_fds.size();
    serialized_fds_.insert(serialized_fds_.end(), serialized_write_fds.begin(),
                          serialized_write_fds.end());
    serialized_write_fds_length_ = serialized_write_fds.size();
    channel_ = nullptr;
  } else {
    // It's valid that the other side wrote some data and closed its end.
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
      // TODO(jam): copied from RawChannelWin::WriteNoLock(
      PlatformHandleVector* all_platform_handles =
          message->transport_data()->platform_handles();
      if (all_platform_handles) {
#if defined(OS_WIN)
        uint64_t* tokens = reinterpret_cast<uint64_t*>(
            static_cast<char*>(message->transport_data()->buffer()) +
            message->transport_data()->platform_handle_table_offset());
        internal::g_broker->HandleToToken(
            &all_platform_handles->at(0), all_platform_handles->size(), tokens);
        for (size_t i = 0; i < all_platform_handles->size(); i++)
          all_platform_handles->at(i) = PlatformHandle();
#else
        for (size_t i = 0; i < all_platform_handles->size(); i++) {
          serialized_fds_.push_back(all_platform_handles->at(i).handle);
          serialized_message_fds_length_++;
          all_platform_handles->at(i) = PlatformHandle();
        }
#endif
      }

      serialized_message_queue_.insert(
          serialized_message_queue_.end(),
          static_cast<const char*>(message->transport_data()->buffer()),
          static_cast<const char*>(message->transport_data()->buffer()) +
              transport_data_buffer_size);
    }

    for (size_t i = 0; i < dispatchers.size(); ++i)
      dispatchers[i]->TransportEnded();
  }
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  SerializeInternal();

  scoped_refptr<MessagePipeDispatcher> rv(
      new MessagePipeDispatcher(transferable_));
  rv->serialized_ = true;
  if (transferable_) {
    rv->serialized_platform_handle_ = std::move(serialized_platform_handle_);
    serialized_message_queue_.swap(rv->serialized_message_queue_);
    serialized_read_buffer_.swap(rv->serialized_read_buffer_);
    serialized_write_buffer_.swap(rv->serialized_write_buffer_);
    serialized_fds_.swap(rv->serialized_fds_);
    rv->serialized_read_fds_length_ = serialized_read_fds_length_;
    rv->serialized_write_fds_length_ = serialized_write_fds_length_;
    rv->serialized_message_fds_length_ = serialized_message_fds_length_;
    rv->write_error_ = write_error_;
  } else {
    rv->pipe_id_ = pipe_id_;
    rv->non_transferable_state_ = non_transferable_state_;
  }
  return rv;
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  lock().AssertAcquired();

  DCHECK(!transports ||
         (transports->size() > 0 &&
          transports->size() <= GetConfiguration().max_message_num_handles));

  if (write_error_ ||
      (transferable_ && !channel_) ||
      (!transferable_ && non_transferable_state_ == CLOSED)) {
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

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
  if (!transferable_)
    message->set_route_id(pipe_id_);
  if (!transferable_ &&
      (non_transferable_state_ == WAITING_FOR_READ_OR_WRITE ||
       non_transferable_state_ == CONNECT_CALLED)) {
    if (non_transferable_state_ == WAITING_FOR_READ_OR_WRITE)
      RequestNontransferableChannel();
    non_transferable_outgoing_message_queue_.AddMessage(std::move(message));
  } else {
    channel_->WriteMessage(std::move(message));
  }

  return MOJO_RESULT_OK;
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes,
    uint32_t* num_bytes,
    DispatcherVector* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();
  if (transferable_ && channel_) {
    channel_->EnsureLazyInitialized();
  } else if (!transferable_) {
    if (non_transferable_state_ == WAITING_FOR_READ_OR_WRITE) {
      RequestNontransferableChannel();
      return MOJO_RESULT_SHOULD_WAIT;
    } else if (non_transferable_state_ == CONNECT_CALLED) {
      return MOJO_RESULT_SHOULD_WAIT;
    }
  }

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
  if (!message_queue_.IsEmpty() ||
      (transferable_ && channel_) ||
      (!transferable_ && non_transferable_state_ != CLOSED))
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_READABLE;
  if (!write_error_ &&
      ((transferable_ && channel_) ||
       (!transferable_ && non_transferable_state_ != CLOSED))) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  }
  if (write_error_ ||
      (transferable_ && !channel_) ||
       (!transferable_ &&
        ((non_transferable_state_ == CLOSED) || is_closed()))) {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

MojoResult MessagePipeDispatcher::AddAwakableImplNoLock(
    Awakable* awakable,
    MojoHandleSignals signals,
    uintptr_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  if (transferable_ && channel_) {
    channel_->EnsureLazyInitialized();
  } else if (!transferable_ &&
             non_transferable_state_ == WAITING_FOR_READ_OR_WRITE) {
    RequestNontransferableChannel();
  }

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
  *max_platform_handles += serialized_fds_.size();
  *max_size = sizeof(SerializedMessagePipeHandleDispatcher);
}

bool MessagePipeDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  CloseImplNoLock();
  SerializedMessagePipeHandleDispatcher* serialization =
      static_cast<SerializedMessagePipeHandleDispatcher*>(destination);
  serialization->transferable = transferable_;
  serialization->pipe_id = pipe_id_;
  if (serialized_platform_handle_.is_valid()) {
    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->platform_handle_index =
        static_cast<uint32_t>(platform_handles->size());
    platform_handles->push_back(serialized_platform_handle_.release());
  } else {
    serialization->platform_handle_index = kInvalidMessagePipeHandleIndex;
  }

  serialization->write_error = write_error_;
  DCHECK(serialized_read_buffer_.size() < std::numeric_limits<uint32_t>::max());
  serialization->serialized_read_buffer_size =
      static_cast<uint32_t>(serialized_read_buffer_.size());
  DCHECK(serialized_write_buffer_.size() <
         std::numeric_limits<uint32_t>::max());
  serialization->serialized_write_buffer_size =
      static_cast<uint32_t>(serialized_write_buffer_.size());
  DCHECK(serialized_message_queue_.size() <
         std::numeric_limits<uint32_t>::max());
  serialization->serialized_message_queue_size =
      static_cast<uint32_t>(serialized_message_queue_.size());

  serialization->shared_memory_size = static_cast<uint32_t>(
      serialization->serialized_read_buffer_size +
      serialization->serialized_write_buffer_size +
      serialization->serialized_message_queue_size);
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

    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->shared_memory_handle_index =
        static_cast<uint32_t>(platform_handles->size());
    platform_handles->push_back(shared_buffer->PassPlatformHandle().release());
  } else {
    serialization->shared_memory_handle_index = kInvalidMessagePipeHandleIndex;
  }

  DCHECK(serialized_read_fds_length_ < std::numeric_limits<uint32_t>::max());
  serialization->serialized_read_fds_length =
      static_cast<uint32_t>(serialized_read_fds_length_);
  DCHECK(serialized_write_fds_length_ < std::numeric_limits<uint32_t>::max());
  serialization->serialized_write_fds_length =
      static_cast<uint32_t>(serialized_write_fds_length_);
  DCHECK(serialized_message_fds_length_ < std::numeric_limits<uint32_t>::max());
  serialization->serialized_message_fds_length =
      static_cast<uint32_t>(serialized_message_fds_length_);
  if (serialized_fds_.empty()) {
    serialization->serialized_fds_index = kInvalidMessagePipeHandleIndex;
  } else {
#if defined(OS_POSIX)
    DCHECK(platform_handles->size() < std::numeric_limits<uint32_t>::max());
    serialization->serialized_fds_index =
        static_cast<uint32_t>(platform_handles->size());
    for (size_t i = 0; i < serialized_fds_.size(); ++i)
      platform_handles->push_back(PlatformHandle(serialized_fds_[i]));
    serialized_fds_.clear();
#endif
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
        message_view.transport_data_buffer_size(),
        std::move(platform_handles)));
  }

  bool call_release = false;
  if (started_transport_.Try()) {
    // We're not in the middle of being sent.

    // Can get synchronously called back in Init if there was initial data.
    scoped_ptr<base::AutoLock> locker;
    if (!calling_init_)
      locker.reset(new base::AutoLock(lock()));

    if (message_view.type() == MessageInTransit::Type::QUIT_MESSAGE) {
      if (transferable_) {
        channel_->Shutdown();
      } else {
        internal::g_broker->CloseMessagePipe(pipe_id_, this);
        non_transferable_state_ = CLOSED;
      }
      channel_ = nullptr;
      awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
      if (close_requested_) {
        // We requested the other side to close the connection while they also
        // did the same. We must balance out the AddRef in CloseOnIO to ensure
        // this object isn't leaked.
        call_release = true;
      }
    } else {
      bool was_empty = message_queue_.IsEmpty();
      message_queue_.AddMessage(std::move(message));
      if (was_empty)
        awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    }

    started_transport_.Release();
  } else {
    if (message_view.type() == MessageInTransit::Type::QUIT_MESSAGE) {
      // We got a request to shutdown the channel but this object is already
      // calling into channel to serialize it. Since all the other side cares
      // about is flushing pending messages, we bounce the quit back to it.
      scoped_ptr<MessageInTransit> message(new MessageInTransit(
          MessageInTransit::Type::QUIT_MESSAGE, 0, nullptr));
      channel_->WriteMessage(std::move(message));
    } else {
      // If RawChannel is calling OnRead, that means it has its read_lock_
      // acquired. That means StartSerialize can't be accessing message queue as
      // it waits on ReleaseHandle first which acquires read_lock_.
      message_queue_.AddMessage(std::move(message));
    }
  }

  if (call_release)
    Release();
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
      DVLOG(1) << "MessagePipeDispatcher read error (connection broken)";
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

  bool call_release = false;
  if (started_transport_.Try()) {
    base::AutoLock locker(lock());
    if (channel_ && error != ERROR_WRITE) {
      if (transferable_) {
        channel_->Shutdown();
      } else {
        CHECK_NE(non_transferable_state_, CLOSED);
        internal::g_broker->CloseMessagePipe(pipe_id_, this);
        non_transferable_state_ = CLOSED;
      }
      channel_ = nullptr;
      if (close_requested_) {
        // Balance AddRef in CloseOnIO.
        call_release = true;
      }
    } else if (!channel_ && !transferable_ &&
               non_transferable_state_ == WAITING_FOR_CONNECT_TO_CLOSE) {
      // Balance AddRef in CloseOnIO.
      call_release = true;
    }
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());
    started_transport_.Release();
  } else {
    // We must be waiting to call ReleaseHandle. It will call Shutdown.
  }

  if (call_release)
    Release();
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
      if (transferable_ && mp->transferable_ &&
          channel_ && mp->channel_ && channel_->IsOtherEndOf(mp->channel_)) {
        // The other case should have been disallowed by |Core|. (Note: |port|
        // is the peer port of the handle given to |WriteMessage()|.)
        return MOJO_RESULT_INVALID_ARGUMENT;
      } else if (!transferable_ && !mp->transferable_ &&
                 pipe_id_ == mp->pipe_id_) {
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
  message->SetDispatchers(std::move(dispatchers));
  return MOJO_RESULT_OK;
}

void MessagePipeDispatcher::RequestNontransferableChannel() {
  lock().AssertAcquired();
  CHECK(!transferable_);
  CHECK_EQ(non_transferable_state_, WAITING_FOR_READ_OR_WRITE);
  non_transferable_state_ = CONNECT_CALLED;
#if !defined(OFFICIAL_BUILD)
  non_transferable_bound_stack_.reset(new base::debug::StackTrace);
#endif

  // PostTask since the broker can call us back synchronously.
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&Broker::ConnectMessagePipe,
                 base::Unretained(internal::g_broker), pipe_id_,
                 base::Unretained(this)));
}

}  // namespace edk
}  // namespace mojo
