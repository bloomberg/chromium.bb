// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/child_broker.h"

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_shared_buffer.h"
#include "mojo/edk/embedder/platform_support.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"

#if defined(OS_POSIX)
#include <fcntl.h>

#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#endif

namespace mojo {
namespace edk {

ChildBroker* ChildBroker::GetInstance() {
  return base::Singleton<
      ChildBroker, base::LeakySingletonTraits<ChildBroker>>::get();
}

void ChildBroker::SetChildBrokerHostHandle(ScopedPlatformHandle handle)  {
  ScopedPlatformHandle parent_async_channel_handle;
  parent_sync_channel_ = std::move(handle);

// We have two pipes to the parent. The first is for the token
// exchange for creating and passing handles on Windows, and creating shared
// buffers on POSIX, since the child needs the parent's help if it is
// sandboxed. The second is used for multiplexing related messages. We
// send the second over the first.
#if defined(OS_POSIX)
  // Make the synchronous channel blocking.
  int flags = fcntl(parent_sync_channel_.get().handle, F_GETFL, 0);
  PCHECK(flags != -1);
  PCHECK(fcntl(parent_sync_channel_.get().handle, F_SETFL,
               flags & ~O_NONBLOCK) != -1);

  std::deque<PlatformHandle> received_handles;
  char buf[1];
  ssize_t result = PlatformChannelRecvmsg(parent_sync_channel_.get(), buf, 1,
                                          &received_handles, true);
  CHECK_EQ(1, result);
  CHECK_EQ(1u, received_handles.size());
  parent_async_channel_handle.reset(received_handles.front());
#else
  HANDLE parent_handle = INVALID_HANDLE_VALUE;
  DWORD bytes_read = 0;
  BOOL rv = ReadFile(parent_sync_channel_.get().handle, &parent_handle,
                     sizeof(parent_handle), &bytes_read, NULL);
  CHECK(rv);
  parent_async_channel_handle.reset(PlatformHandle(parent_handle));
#endif
  sync_channel_lock_.Unlock();

  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&ChildBroker::InitAsyncChannel, base::Unretained(this),
                 base::Passed(&parent_async_channel_handle)));
}

#if defined(OS_WIN)
void ChildBroker::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  sync_channel_lock_.Lock();
  CreatePlatformChannelPairNoLock(server, client);
  sync_channel_lock_.Unlock();
}

void ChildBroker::HandleToToken(const PlatformHandle* platform_handles,
                                size_t count,
                                uint64_t* tokens) {
  uint32_t size = kBrokerMessageHeaderSize +
                  static_cast<int>(count) * sizeof(HANDLE);
  std::vector<char> message_buffer(size);
  BrokerMessage* message = reinterpret_cast<BrokerMessage*>(&message_buffer[0]);
  message->size = size;
  message->id = HANDLE_TO_TOKEN;
  for (size_t i = 0; i < count; ++i)
    message->handles[i] = platform_handles[i].handle;

  uint32_t response_size = static_cast<int>(count) * sizeof(uint64_t);
  sync_channel_lock_.Lock();
  WriteAndReadResponse(message, tokens, response_size);
  sync_channel_lock_.Unlock();
}

void ChildBroker::TokenToHandle(const uint64_t* tokens,
                                size_t count,
                                PlatformHandle* handles) {
  uint32_t size = kBrokerMessageHeaderSize +
                  static_cast<int>(count) * sizeof(uint64_t);
  std::vector<char> message_buffer(size);
  BrokerMessage* message =
      reinterpret_cast<BrokerMessage*>(&message_buffer[0]);
  message->size = size;
  message->id = TOKEN_TO_HANDLE;
  memcpy(&message->tokens[0], tokens, count * sizeof(uint64_t));

  std::vector<HANDLE> handles_temp(count);
  uint32_t response_size =
      static_cast<uint32_t>(handles_temp.size()) * sizeof(HANDLE);
  sync_channel_lock_.Lock();
  if (WriteAndReadResponse(message, &handles_temp[0], response_size)) {
    for (uint32_t i = 0; i < count; ++i)
      handles[i].handle = handles_temp[i];
    sync_channel_lock_.Unlock();
  }
}
#else
scoped_refptr<PlatformSharedBuffer> ChildBroker::CreateSharedBuffer(
    size_t num_bytes) {
  sync_channel_lock_.Lock();
  scoped_refptr<PlatformSharedBuffer> shared_buffer;

  BrokerMessage message;
  message.size = kBrokerMessageHeaderSize + sizeof(uint32_t);
  message.id = CREATE_SHARED_BUFFER;
  message.shared_buffer_size = num_bytes;

  std::deque<PlatformHandle> handles;
  if (WriteAndReadHandles(&message, &handles)) {
    DCHECK_EQ(1u, handles.size());
    PlatformHandle handle = handles.front();
    if (handle.is_valid())
      shared_buffer =
          internal::g_platform_support->CreateSharedBufferFromHandle(
              num_bytes, ScopedPlatformHandle(handles.front()));
  }
  sync_channel_lock_.Unlock();
  return shared_buffer;
}
#endif

void ChildBroker::ConnectMessagePipe(uint64_t pipe_id,
                                     MessagePipeDispatcher* message_pipe) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());

  ConnectMessagePipeMessage data;
  memset(&data, 0, sizeof(data));
  data.pipe_id = pipe_id;
  if (pending_connects_.find(pipe_id) != pending_connects_.end()) {
    if (!parent_async_channel_) {
      // On Windows, we can't create the local RoutedRawChannel yet because we
      // don't have parent_sync_channel_. Treat all platforms the same and just
      // queue this.
      CHECK(pending_inprocess_connects_.find(pipe_id) ==
            pending_inprocess_connects_.end());
      pending_inprocess_connects_[pipe_id] = message_pipe;
      return;
    }
    // Both ends of the message pipe are in the same process.
    // First, tell the browser side that to remove its bookkeeping for a pending
    // connect, since it'll never get the other side.

    data.type = CANCEL_CONNECT_MESSAGE_PIPE;
    scoped_ptr<MessageInTransit> message(new MessageInTransit(
        MessageInTransit::Type::MESSAGE, sizeof(data), &data));
    WriteAsyncMessage(std::move(message));

    if (!in_process_pipes_channel1_) {
      ScopedPlatformHandle server_handle, client_handle;
#if defined(OS_WIN)
      CreatePlatformChannelPairNoLock(&server_handle, &client_handle);
#else
      PlatformChannelPair channel_pair;
      server_handle = channel_pair.PassServerHandle();
      client_handle = channel_pair.PassClientHandle();
#endif
      in_process_pipes_channel1_ = new RoutedRawChannel(
          std::move(server_handle),
          base::Bind(&ChildBroker::ChannelDestructed, base::Unretained(this)));
      in_process_pipes_channel2_ = new RoutedRawChannel(
          std::move(client_handle),
          base::Bind(&ChildBroker::ChannelDestructed, base::Unretained(this)));
    }

    AttachMessagePipe(pending_connects_[pipe_id], pipe_id,
                      in_process_pipes_channel1_);
    AttachMessagePipe(message_pipe, pipe_id, in_process_pipes_channel2_);
    pending_connects_.erase(pipe_id);
    return;
  }

  data.type = CONNECT_MESSAGE_PIPE;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::Type::MESSAGE, sizeof(data), &data));
  pending_connects_[pipe_id] = message_pipe;
  WriteAsyncMessage(std::move(message));
}

void ChildBroker::CloseMessagePipe(
    uint64_t pipe_id, MessagePipeDispatcher* message_pipe) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  CHECK(connected_pipes_.find(message_pipe) != connected_pipes_.end());
  connected_pipes_[message_pipe]->RemoveRoute(pipe_id);
  connected_pipes_.erase(message_pipe);
}

ChildBroker::ChildBroker()
    : parent_async_channel_(nullptr),
      in_process_pipes_channel1_(nullptr),
      in_process_pipes_channel2_(nullptr) {
  DCHECK(!internal::g_broker);
  internal::g_broker = this;
  // Block any threads from calling this until we have a pipe to the parent.
  sync_channel_lock_.Lock();
}

ChildBroker::~ChildBroker() {
}

void ChildBroker::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  MultiplexMessages type =
      *static_cast<const MultiplexMessages*>(message_view.bytes());
  if (type == CONNECT_TO_PROCESS) {
    DCHECK_EQ(platform_handles->size(), 1u);
    ScopedPlatformHandle handle((*platform_handles.get())[0]);
    (*platform_handles.get())[0] = PlatformHandle();

    const ConnectToProcessMessage* message =
        static_cast<const ConnectToProcessMessage*>(message_view.bytes());

    CHECK(channels_.find(message->process_id) == channels_.end());
    channels_[message->process_id] = new RoutedRawChannel(
        std::move(handle),
        base::Bind(&ChildBroker::ChannelDestructed, base::Unretained(this)));
  } else if (type == PEER_PIPE_CONNECTED) {
    DCHECK(!platform_handles);
    const PeerPipeConnectedMessage* message =
        static_cast<const PeerPipeConnectedMessage*>(message_view.bytes());

    uint64_t pipe_id = message->pipe_id;
    uint64_t peer_pid = message->process_id;

    CHECK(pending_connects_.find(pipe_id) != pending_connects_.end());
    MessagePipeDispatcher* pipe = pending_connects_[pipe_id];
    pending_connects_.erase(pipe_id);
    if (peer_pid == 0) {
      // The other side is in the parent process.
      AttachMessagePipe(pipe, pipe_id, parent_async_channel_);
    } else if (channels_.find(peer_pid) == channels_.end()) {
      // We saw the peer process die before we got the reply from the parent.
      pipe->OnError(ERROR_READ_SHUTDOWN);
    } else {
      CHECK(connected_pipes_.find(pipe) == connected_pipes_.end());
      AttachMessagePipe(pipe, pipe_id, channels_[peer_pid]);
    }
  } else if (type == PEER_DIED) {
    DCHECK(!platform_handles);
    const PeerDiedMessage* message =
        static_cast<const PeerDiedMessage*>(message_view.bytes());

    uint64_t pipe_id = message->pipe_id;

    CHECK(pending_connects_.find(pipe_id) != pending_connects_.end());
    MessagePipeDispatcher* pipe = pending_connects_[pipe_id];
    pending_connects_.erase(pipe_id);
    pipe->OnError(ERROR_READ_SHUTDOWN);
  } else {
    NOTREACHED();
  }
}

void ChildBroker::OnError(Error error) {
  // The parent process shut down.
}

void ChildBroker::ChannelDestructed(RoutedRawChannel* channel) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  for (auto it : channels_) {
    if (it.second == channel) {
      channels_.erase(it.first);
      break;
    }
  }
}

void ChildBroker::WriteAsyncMessage(scoped_ptr<MessageInTransit> message) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  message->set_route_id(kBrokerRouteId);
  if (parent_async_channel_) {
    parent_async_channel_->channel()->WriteMessage(std::move(message));
  } else {
    async_channel_queue_.AddMessage(std::move(message));
  }
}

void ChildBroker::InitAsyncChannel(
    ScopedPlatformHandle parent_async_channel_handle) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());

  parent_async_channel_ = new RoutedRawChannel(
      std::move(parent_async_channel_handle),
      base::Bind(&ChildBroker::ChannelDestructed, base::Unretained(this)));
  parent_async_channel_->AddRoute(kBrokerRouteId, this);
  while (!async_channel_queue_.IsEmpty()) {
    parent_async_channel_->channel()->WriteMessage(
        async_channel_queue_.GetMessage());
  }

  while (!pending_inprocess_connects_.empty()) {
    ConnectMessagePipe(pending_inprocess_connects_.begin()->first,
                       pending_inprocess_connects_.begin()->second);
    pending_inprocess_connects_.erase(pending_inprocess_connects_.begin());
  }
}

void ChildBroker::AttachMessagePipe(MessagePipeDispatcher* message_pipe,
                                    uint64_t pipe_id,
                                    RoutedRawChannel* raw_channel) {
  connected_pipes_[message_pipe] = raw_channel;
  // Note: we must call GotNonTransferableChannel before AddRoute because there
  // could be race conditions if the pipe got queued messages in |AddRoute| but
  // then when it's read it returns no messages because it doesn't have the
  // channel yet.
  message_pipe->GotNonTransferableChannel(raw_channel->channel());
  // The above call could have caused |CloseMessagePipe| to be called.
  if (connected_pipes_.find(message_pipe) != connected_pipes_.end())
    raw_channel->AddRoute(pipe_id, message_pipe);
}

#if defined(OS_WIN)

void ChildBroker::CreatePlatformChannelPairNoLock(
    ScopedPlatformHandle* server,
    ScopedPlatformHandle* client) {
  BrokerMessage message;
  message.size = kBrokerMessageHeaderSize;
  message.id = CREATE_PLATFORM_CHANNEL_PAIR;

  uint32_t response_size = 2 * sizeof(HANDLE);
  HANDLE handles[2];
  if (WriteAndReadResponse(&message, handles, response_size)) {
    server->reset(PlatformHandle(handles[0]));
    client->reset(PlatformHandle(handles[1]));
  }
}

bool ChildBroker::WriteAndReadResponse(BrokerMessage* message,
                                       void* response,
                                       uint32_t response_size) {
  CHECK(parent_sync_channel_.is_valid());

  bool result = true;
  DWORD bytes_written = 0;
  // This will always write in one chunk per
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365150.aspx.
  BOOL rv = WriteFile(parent_sync_channel_.get().handle, message, message->size,
                      &bytes_written, NULL);
  if (!rv || bytes_written != message->size) {
    LOG(ERROR) << "Child token serializer couldn't write message.";
    result = false;
  } else {
    while (response_size) {
      DWORD bytes_read = 0;
      rv = ReadFile(parent_sync_channel_.get().handle, response, response_size,
                    &bytes_read, NULL);
      if (!rv) {
        LOG(ERROR) << "Child token serializer couldn't read result.";
        result = false;
        break;
      }
      response_size -= bytes_read;
      response = static_cast<char*>(response) + bytes_read;
    }
  }

  return result;
}
#else
bool ChildBroker::WriteAndReadHandles(BrokerMessage* message,
                                      std::deque<PlatformHandle>* handles) {
  CHECK(parent_sync_channel_.is_valid());

  uint32_t remaining_bytes = message->size;
  while (remaining_bytes > 0) {
    ssize_t bytes_written = PlatformChannelWrite(
        parent_sync_channel_.get(),
        reinterpret_cast<uint8_t*>(message) + (message->size - remaining_bytes),
        remaining_bytes);

    if (bytes_written != -1)
      remaining_bytes -= bytes_written;
    else
      return false;
  }
  // Perform a blocking read (we set this fd to be blocking in
  // SetChildBrokerHostHandle).
  char buf[1];
  ssize_t bytes_read = PlatformChannelRecvmsg(
      parent_sync_channel_.get(), buf, 1, handles, true /* should_block */);
  // If the other side shutdown, or there was an error, or we didn't get
  // any handles.
  if (bytes_read == 0 || bytes_read == -1 || handles->empty())
    return false;

  return true;
}
#endif

}  // namespace edk
}  // namespace mojo
