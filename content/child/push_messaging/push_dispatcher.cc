// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/push_messaging/push_dispatcher.h"

#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "content/child/push_messaging/push_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/common/push_messaging_messages.h"

namespace content {

PushDispatcher::PushDispatcher(ThreadSafeSender* thread_safe_sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(thread_safe_sender),
      next_request_id_(0) {
}

PushDispatcher::~PushDispatcher() {
}

int PushDispatcher::GenerateRequestId(int thread_id) {
  base::AutoLock lock(request_id_map_lock_);
  request_id_map_[next_request_id_] = thread_id;
  return next_request_id_++;
}

base::TaskRunner* PushDispatcher::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (!ShouldHandleMessage(msg))
    return nullptr;

  int request_id = -1;
  int thread_id = 0;

  const bool success = PickleIterator(msg).ReadInt(&request_id);
  DCHECK(success);

  {
    base::AutoLock lock(request_id_map_lock_);
    auto it = request_id_map_.find(request_id);
    if (it != request_id_map_.end()) {
      thread_id = it->second;
      request_id_map_.erase(it);
    }
  }

  if (!thread_id)
    return main_thread_loop_proxy_.get();

  return new WorkerThreadTaskRunner(thread_id);
}

bool PushDispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (!ShouldHandleMessage(msg))
    return false;

  bool handled = PushProvider::ThreadSpecificInstance(
                     thread_safe_sender_.get(), this)->OnMessageReceived(msg);
  DCHECK(handled);
  return handled;
}

bool PushDispatcher::ShouldHandleMessage(const IPC::Message& msg) {
  // Note that not all Push API IPC messages flow through this class. A subset
  // of the API functionality requires a direct association with a document and
  // a frame, and for those cases the IPC messages are handled by a
  // RenderFrameObserver.
  return msg.type() == PushMessagingMsg_RegisterFromWorkerSuccess::ID ||
         msg.type() == PushMessagingMsg_RegisterFromWorkerError::ID ||
         msg.type() == PushMessagingMsg_GetPermissionStatusSuccess::ID ||
         msg.type() == PushMessagingMsg_GetPermissionStatusError::ID ||
         msg.type() == PushMessagingMsg_UnregisterSuccess::ID ||
         msg.type() == PushMessagingMsg_UnregisterError::ID;
}

}  // namespace content
