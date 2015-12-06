// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/broker_state.h"

#include "base/bind.h"
#include "base/rand_util.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/child_broker_host.h"
#include "mojo/edk/system/message_pipe_dispatcher.h"
#include "mojo/edk/system/routed_raw_channel.h"

namespace mojo {
namespace edk {

BrokerState* BrokerState::GetInstance() {
  return base::Singleton<
      BrokerState, base::LeakySingletonTraits<BrokerState>>::get();
}

#if defined(OS_WIN)
void BrokerState::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  PlatformChannelPair channel_pair;
  *server = channel_pair.PassServerHandle();
  *client = channel_pair.PassClientHandle();
}

void BrokerState::HandleToToken(
    const PlatformHandle* platform_handles,
    size_t count,
    uint64_t* tokens) {
  base::AutoLock auto_locker(token_map_lock_);
  for (size_t i = 0; i < count; ++i) {
    if (platform_handles[i].is_valid()) {
      uint64_t token;
      do {
        token = base::RandUint64();
      } while (!token || token_map_.find(token) != token_map_.end());
      tokens[i] = token;
      token_map_[tokens[i]] = platform_handles[i].handle;
    } else {
      DLOG(WARNING) << "BrokerState got invalid handle.";
      tokens[i] = 0;
    }
  }
}

void BrokerState::TokenToHandle(const uint64_t* tokens,
                                size_t count,
                                PlatformHandle* handles) {
  base::AutoLock auto_locker(token_map_lock_);
  for (size_t i = 0; i < count; ++i) {
    auto it = token_map_.find(tokens[i]);
    if (it == token_map_.end()) {
      DLOG(WARNING) << "TokenToHandle didn't find token.";
    } else {
      handles[i].handle = it->second;
      token_map_.erase(it);
    }
  }
}
#endif

void BrokerState::ConnectMessagePipe(uint64_t pipe_id,
                                     MessagePipeDispatcher* message_pipe) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (pending_connects_.find(pipe_id) != pending_connects_.end()) {
    // Both ends of the message pipe are in this process.
    if (!in_process_pipes_channel1_) {
      PlatformChannelPair channel_pair;
      in_process_pipes_channel1_ = new RoutedRawChannel(
          channel_pair.PassServerHandle(),
          base::Bind(&BrokerState::ChannelDestructed, base::Unretained(this)));
      in_process_pipes_channel2_ = new RoutedRawChannel(
          channel_pair.PassClientHandle(),
          base::Bind(&BrokerState::ChannelDestructed, base::Unretained(this)));
    }

    connected_pipes_[pending_connects_[pipe_id]] = in_process_pipes_channel1_;
    connected_pipes_[message_pipe] = in_process_pipes_channel2_;
    in_process_pipes_channel1_->AddRoute(pipe_id, pending_connects_[pipe_id]);
    in_process_pipes_channel2_->AddRoute(pipe_id, message_pipe);
    pending_connects_[pipe_id]->GotNonTransferableChannel(
        in_process_pipes_channel1_->channel());
    message_pipe->GotNonTransferableChannel(
        in_process_pipes_channel2_->channel());

    pending_connects_.erase(pipe_id);
    return;
  }

  if (pending_child_connects_.find(pipe_id) != pending_child_connects_.end()) {
    // A child process has already tried to connect.
    EnsureProcessesConnected(base::GetCurrentProcId(),
                             pending_child_connects_[pipe_id]->GetProcessId());
    pending_child_connects_[pipe_id]->ConnectMessagePipe(
        pipe_id, base::GetCurrentProcId());
    base::ProcessId peer_pid = pending_child_connects_[pipe_id]->GetProcessId();
    pending_child_connects_.erase(pipe_id);
    connected_pipes_[message_pipe] = child_channels_[peer_pid];
    child_channels_[peer_pid]->AddRoute(pipe_id, message_pipe);
    message_pipe->GotNonTransferableChannel(
        child_channels_[peer_pid]->channel());
    return;
  }

  pending_connects_[pipe_id] = message_pipe;
}

void BrokerState::CloseMessagePipe(uint64_t pipe_id,
                                   MessagePipeDispatcher* message_pipe) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);

  CHECK(connected_pipes_.find(message_pipe) != connected_pipes_.end());
  connected_pipes_[message_pipe]->RemoveRoute(pipe_id, message_pipe);
  connected_pipes_.erase(message_pipe);
}

void BrokerState::ChildBrokerHostCreated(ChildBrokerHost* child_broker_host) {
  base::AutoLock auto_lock(lock_);
  CHECK(child_processes_.find(child_broker_host->GetProcessId()) ==
        child_processes_.end());
  child_processes_[child_broker_host->GetProcessId()] = child_broker_host;
}

void BrokerState::ChildBrokerHostDestructed(
    ChildBrokerHost* child_broker_host) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);

  for (auto it = pending_child_connects_.begin();
       it != pending_child_connects_.end();) {
    if (it->second == child_broker_host) {
      // Since we can't do it = pending_child_connects_.erase(it); until
      // hash_map uses unordered_map on posix.
      auto cur = it++;
      pending_child_connects_.erase(cur);
    } else {
      it++;
    }
  }

  base::ProcessId pid = child_broker_host->GetProcessId();
  for (auto it = connected_processes_.begin();
       it != connected_processes_.end();) {
    if ((*it).first == pid || (*it).second == pid) {
      // Since we can't do it = pending_child_connects_.erase(it); until
      // hash_map uses unordered_map on posix.
      auto cur = it++;
      connected_processes_.erase(cur);
    } else {
      it++;
    }
  }

  CHECK(child_processes_.find(pid) != child_processes_.end());
  child_processes_.erase(pid);
}

void BrokerState::HandleConnectMessagePipe(ChildBrokerHost* pipe_process,
                                           uint64_t pipe_id) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (pending_child_connects_.find(pipe_id) != pending_child_connects_.end()) {
    // Another child process is waiting to connect to the given pipe.
    ChildBrokerHost* pending_pipe_process = pending_child_connects_[pipe_id];
    EnsureProcessesConnected(pipe_process->GetProcessId(),
                             pending_pipe_process->GetProcessId());
    pending_pipe_process->ConnectMessagePipe(
        pipe_id, pipe_process->GetProcessId());
    pipe_process->ConnectMessagePipe(
        pipe_id, pending_pipe_process->GetProcessId());
    pending_child_connects_.erase(pipe_id);
    return;
  }

  if (pending_connects_.find(pipe_id) != pending_connects_.end()) {
    // This parent process is the other side of the given pipe.
    EnsureProcessesConnected(base::GetCurrentProcId(),
                             pipe_process->GetProcessId());
    MessagePipeDispatcher* pending_pipe = pending_connects_[pipe_id];
    connected_pipes_[pending_pipe] =
        child_channels_[pipe_process->GetProcessId()];
    child_channels_[pipe_process->GetProcessId()]->AddRoute(
        pipe_id, pending_pipe);
    pending_pipe->GotNonTransferableChannel(
        child_channels_[pipe_process->GetProcessId()]->channel());
    pipe_process->ConnectMessagePipe(
        pipe_id, base::GetCurrentProcId());
    pending_connects_.erase(pipe_id);
    return;
  }

  // This is the first connection request for pipe_id to reach the parent.
  pending_child_connects_[pipe_id] = pipe_process;
}

void BrokerState::HandleCancelConnectMessagePipe(uint64_t pipe_id) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);
  if (pending_child_connects_.find(pipe_id) == pending_child_connects_.end()) {
    NOTREACHED() << "Can't find entry for pipe_id " << pipe_id;
  } else {
    pending_child_connects_.erase(pipe_id);
  }
}

BrokerState::BrokerState()
    : in_process_pipes_channel1_(nullptr),
      in_process_pipes_channel2_(nullptr) {
  DCHECK(!internal::g_broker);
  internal::g_broker = this;
}

BrokerState::~BrokerState() {
}

void BrokerState::EnsureProcessesConnected(base::ProcessId pid1,
                                           base::ProcessId pid2) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  lock_.AssertAcquired();
  CHECK_NE(pid1, pid2);
  CHECK_NE(pid2, base::GetCurrentProcId());
  std::pair<base::ProcessId, base::ProcessId> processes;
  processes.first = std::min(pid1, pid2);
  processes.second = std::max(pid1, pid2);
  if (connected_processes_.find(processes) != connected_processes_.end())
    return;

  connected_processes_.insert(processes);
  PlatformChannelPair channel_pair;
  if (pid1 == base::GetCurrentProcId()) {
    CHECK(child_channels_.find(pid2) == child_channels_.end());
    CHECK(child_processes_.find(pid2) != child_processes_.end());
    child_channels_[pid2] = new RoutedRawChannel(
        channel_pair.PassServerHandle(),
        base::Bind(&BrokerState::ChannelDestructed, base::Unretained(this)));
    child_processes_[pid2]->ConnectToProcess(base::GetCurrentProcId(),
                                             channel_pair.PassClientHandle());
    return;
  }

  CHECK(child_processes_.find(pid1) != child_processes_.end());
  CHECK(child_processes_.find(pid2) != child_processes_.end());
  child_processes_[pid1]->ConnectToProcess(pid2,
                                           channel_pair.PassServerHandle());
  child_processes_[pid2]->ConnectToProcess(pid1,
                                           channel_pair.PassClientHandle());
}

void BrokerState::ChannelDestructed(RoutedRawChannel* channel) {
  DCHECK(internal::g_io_thread_task_runner->RunsTasksOnCurrentThread());
  base::AutoLock auto_lock(lock_);
  for (auto it : child_channels_) {
    if (it.second == channel) {
      child_channels_.erase(it.first);
      break;
    }
  }
}

}  // namespace edk
}  // namespace mojo
