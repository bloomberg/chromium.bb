// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/child_broker_host.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/broker_state.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/core.h"
#include "mojo/edk/system/platform_handle_dispatcher.h"

namespace mojo {
namespace edk {

namespace {
#if defined(OS_WIN)
static const int kDefaultReadBufferSize = 256;
#endif
}

ChildBrokerHost::ChildBrokerHost(base::ProcessHandle child_process,
                                 ScopedPlatformHandle pipe)
    : process_id_(base::GetProcId(child_process)), child_channel_(nullptr) {
  ScopedPlatformHandle parent_async_channel_handle;
#if defined(OS_POSIX)
  parent_async_channel_handle = pipe.Pass();
#else
  DuplicateHandle(GetCurrentProcess(), child_process,
                  GetCurrentProcess(), &child_process,
                  0, FALSE, DUPLICATE_SAME_ACCESS);
  child_process_ = base::Process(child_process);
  sync_channel_ = pipe.Pass();
  memset(&read_context_.overlapped, 0, sizeof(read_context_.overlapped));
  read_context_.handler = this;
  memset(&write_context_.overlapped, 0, sizeof(write_context_.overlapped));
  write_context_.handler = this;
  read_data_.resize(kDefaultReadBufferSize);
  num_bytes_read_ = 0;

  // See comment in ChildBroker::SetChildBrokerHostHandle. Summary is we need
  // two pipes on Windows, so send the second one over the first one.
  PlatformChannelPair parent_pipe;
  parent_async_channel_handle = parent_pipe.PassServerHandle();

  HANDLE duplicated_child_handle =
      DuplicateToChild(parent_pipe.PassClientHandle().release().handle);
  BOOL rv = WriteFile(sync_channel_.get().handle,
                      &duplicated_child_handle, sizeof(duplicated_child_handle),
                      NULL, &write_context_.overlapped);
  DCHECK(rv || GetLastError() == ERROR_IO_PENDING);
#endif

  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&ChildBrokerHost::InitOnIO, base::Unretained(this),
                 base::Passed(&parent_async_channel_handle)));
}

base::ProcessId ChildBrokerHost::GetProcessId() {
  return process_id_;
}

void ChildBrokerHost::ConnectToProcess(base::ProcessId process_id,
                                       ScopedPlatformHandle pipe) {
  if (!child_channel_)
    return;  // Can happen at process shutdown on Windows.
  ConnectToProcessMessage data;
  data.type = CONNECT_TO_PROCESS;
  data.process_id = process_id;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::Type::MESSAGE, sizeof(data), &data));
  scoped_refptr<Dispatcher> dispatcher =
      PlatformHandleDispatcher::Create(pipe.Pass());
  internal::g_core->AddDispatcher(dispatcher);
  scoped_ptr<DispatcherVector> dispatchers(new DispatcherVector);
  dispatchers->push_back(dispatcher);
  message->SetDispatchers(dispatchers.Pass());
  message->SerializeAndCloseDispatchers();
  message->set_route_id(kBrokerRouteId);
  child_channel_->channel()->WriteMessage(message.Pass());
}

void ChildBrokerHost::ConnectMessagePipe(uint64_t pipe_id,
                                         base::ProcessId process_id) {
  if (!child_channel_)
    return;  // Can happen at process shutdown on Windows.
  PeerPipeConnectedMessage data;
  data.type = PEER_PIPE_CONNECTED;
  data.pipe_id = pipe_id;
  data.process_id = process_id;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::Type::MESSAGE, sizeof(data), &data));
  message->set_route_id(kBrokerRouteId);
  child_channel_->channel()->WriteMessage(message.Pass());
}

ChildBrokerHost::~ChildBrokerHost() {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  BrokerState::GetInstance()->ChildBrokerHostDestructed(this);
  if (child_channel_)
    child_channel_->RemoveRoute(kBrokerRouteId);
}

void ChildBrokerHost::InitOnIO(
    ScopedPlatformHandle parent_async_channel_handle) {
  child_channel_ = new RoutedRawChannel(
      parent_async_channel_handle.Pass(),
      base::Bind(&ChildBrokerHost::ChannelDestructed, base::Unretained(this)));
  child_channel_->AddRoute(kBrokerRouteId, this);

  BrokerState::GetInstance()->ChildBrokerHostCreated(this);

#if defined(OS_WIN)
  // Call this after RoutedRawChannel calls its RawChannel::Init because this
  // call could cause us to get notified that the child has gone away and to
  // delete this class and shut down child_channel_ before Init() has run.
  base::MessageLoopForIO::current()->RegisterIOHandler(
      sync_channel_.get().handle, this);
  BeginRead();
#endif
}

void ChildBrokerHost::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  CHECK(!platform_handles);
  if (message_view.num_bytes() !=
      static_cast<uint32_t>(sizeof(ConnectMessagePipeMessage))) {
    NOTREACHED();
    delete this;
  }

  const ConnectMessagePipeMessage* message =
      static_cast<const ConnectMessagePipeMessage*>(message_view.bytes());
  switch(message->type) {
    case CONNECT_MESSAGE_PIPE:
      BrokerState::GetInstance()->HandleConnectMessagePipe(this,
                                                           message->pipe_id);
      break;
    case CANCEL_CONNECT_MESSAGE_PIPE:
      BrokerState::GetInstance()->HandleCancelConnectMessagePipe(
          message->pipe_id);
      break;
    default:
      NOTREACHED();
      delete this;
  }
}

void ChildBrokerHost::OnError(Error error) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  child_channel_->RemoveRoute(kBrokerRouteId);
  child_channel_ = nullptr;
}

void ChildBrokerHost::ChannelDestructed(RoutedRawChannel* channel) {
  // On Windows, we have two pipes to the child process. It's easier to wait
  // until we get the error from the pipe that is used for synchronous I/O.
#if !defined(OS_WIN)
  delete this;
#endif
}

#if defined(OS_WIN)
void ChildBrokerHost::BeginRead() {
  BOOL rv = ReadFile(sync_channel_.get().handle,
                     &read_data_[num_bytes_read_],
                     static_cast<int>(read_data_.size() - num_bytes_read_),
                     nullptr, &read_context_.overlapped);
  if (rv || GetLastError() == ERROR_IO_PENDING)
    return;

  if (GetLastError() == ERROR_BROKEN_PIPE) {
    delete this;
    return;
  }

  NOTREACHED() << "Unknown error in ChildBrokerHost " << rv;
}

void ChildBrokerHost::OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                                    DWORD bytes_transferred,
                                    DWORD error) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  if (error == ERROR_BROKEN_PIPE) {
    delete this;
    return;  // Child process exited or crashed.
  }

  if (error != ERROR_SUCCESS) {
    NOTREACHED() << "Error " << error << " in ChildBrokerHost.";
    delete this;
    return;
  }

  if (context == &write_context_) {
    write_data_.clear();
    return;
  }

  num_bytes_read_ += bytes_transferred;
  CHECK_GE(num_bytes_read_, sizeof(uint32_t));
  BrokerMessage* message = reinterpret_cast<BrokerMessage*>(&read_data_[0]);
  if (num_bytes_read_ < message->size) {
    read_data_.resize(message->size);
    BeginRead();
    return;
  }

  // This should never fire because we only get new requests from a child
  // process after it has read all the previous data we wrote.
  if (!write_data_.empty()) {
    NOTREACHED() << "ChildBrokerHost shouldn't have data to write when it gets "
                 << " a new request";
    delete this;
    return;
  }

  if (message->id == CREATE_PLATFORM_CHANNEL_PAIR) {
    PlatformChannelPair channel_pair;
    uint32_t response_size = 2 * sizeof(HANDLE);
    write_data_.resize(response_size);
    HANDLE* handles = reinterpret_cast<HANDLE*>(&write_data_[0]);
    handles[0] = DuplicateToChild(
        channel_pair.PassServerHandle().release().handle);
    handles[1] = DuplicateToChild(
        channel_pair.PassClientHandle().release().handle);
  } else if (message->id == HANDLE_TO_TOKEN) {
    uint32_t count =
        (message->size - kBrokerMessageHeaderSize) / sizeof(HANDLE);
    if (count > GetConfiguration().max_message_num_handles) {
      NOTREACHED() << "Too many handles from child process. Closing channel.";
      delete this;
      return;
    }
    uint32_t response_size = count * sizeof(uint64_t);
    write_data_.resize(response_size);
    uint64_t* tokens = reinterpret_cast<uint64_t*>(&write_data_[0]);
    std::vector<PlatformHandle> duplicated_handles(count);
    for (uint32_t i = 0; i < count; ++i) {
      duplicated_handles[i] =
          PlatformHandle(DuplicateFromChild(message->handles[i]));
    }
    BrokerState::GetInstance()->HandleToToken(
        &duplicated_handles[0], count, tokens);
  } else if (message->id == TOKEN_TO_HANDLE) {
    uint32_t count =
        (message->size - kBrokerMessageHeaderSize) / sizeof(uint64_t);
    if (count > GetConfiguration().max_message_num_handles) {
      NOTREACHED() << "Too many tokens from child process. Closing channel.";
      delete this;
      return;
    }
    uint32_t response_size = count * sizeof(HANDLE);
    write_data_.resize(response_size);
    HANDLE* handles = reinterpret_cast<HANDLE*>(&write_data_[0]);
    std::vector<PlatformHandle> temp_handles(count);
    BrokerState::GetInstance()->TokenToHandle(
        &message->tokens[0], count, &temp_handles[0]);
    for (uint32_t i = 0; i < count; ++i) {
      if (temp_handles[i].is_valid()) {
        handles[i] = DuplicateToChild(temp_handles[i].handle);
      } else {
        NOTREACHED() << "Unknown token";
        handles[i] = INVALID_HANDLE_VALUE;
      }
    }
  } else {
    NOTREACHED() << "Unknown command. Stopping reading.";
    delete this;
    return;
  }

  BOOL rv = WriteFile(sync_channel_.get().handle, &write_data_[0],
                      static_cast<int>(write_data_.size()), NULL,
                      &write_context_.overlapped);
  DCHECK(rv || GetLastError() == ERROR_IO_PENDING);

  // Start reading again.
  num_bytes_read_ = 0;
  BeginRead();
}

HANDLE ChildBrokerHost::DuplicateToChild(HANDLE handle) {
  HANDLE rv = INVALID_HANDLE_VALUE;
  BOOL result = DuplicateHandle(base::GetCurrentProcessHandle(), handle,
                                child_process_.Handle(), &rv, 0, FALSE,
                                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  DCHECK(result);
  return rv;
}

HANDLE ChildBrokerHost::DuplicateFromChild(HANDLE handle) {
  HANDLE rv = INVALID_HANDLE_VALUE;
  BOOL result = DuplicateHandle(child_process_.Handle(), handle,
                                base::GetCurrentProcessHandle(), &rv, 0, FALSE,
                                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  DCHECK(result);
  return rv;
}
#endif

}  // namespace edk
}  // namespace mojo
