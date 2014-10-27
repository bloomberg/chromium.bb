// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_dispatcher.h"

#include "base/message_loop/message_loop_proxy.h"
#include "content/child/notifications/notification_manager.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_thread_task_runner.h"
#include "content/common/platform_notification_messages.h"

namespace content {

NotificationDispatcher::NotificationDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : main_thread_loop_proxy_(base::MessageLoopProxy::current()),
      thread_safe_sender_(thread_safe_sender) {
}

NotificationDispatcher::~NotificationDispatcher() {}

int NotificationDispatcher::GenerateNotificationId(int thread_id) {
  base::AutoLock lock(notification_id_map_lock_);
  notification_id_map_[next_notification_id_] = thread_id;
  return next_notification_id_++;
}

base::TaskRunner* NotificationDispatcher::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  if (!ShouldHandleMessage(msg))
    return NULL;

  int notification_id = -1,
      thread_id = 0;

  const bool success = PickleIterator(msg).ReadInt(&notification_id);
  DCHECK(success);

  {
    base::AutoLock lock(notification_id_map_lock_);
    auto iterator = notification_id_map_.find(notification_id);
    if (iterator != notification_id_map_.end())
      thread_id = iterator->second;
  }

  if (!thread_id)
    return main_thread_loop_proxy_.get();

  return new WorkerThreadTaskRunner(thread_id);
}

bool NotificationDispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (!ShouldHandleMessage(msg))
    return false;

  NotificationManager::ThreadSpecificInstance(thread_safe_sender_.get(), this)
      ->OnMessageReceived(msg);
  return true;
}

bool NotificationDispatcher::ShouldHandleMessage(const IPC::Message& msg) {
  // The thread-safe message filter is responsible for handling all the messages
  // except for the routed permission-request-completed message, which will be
  // picked up by the RenderFrameImpl instead.
  return IPC_MESSAGE_CLASS(msg) == PlatformNotificationMsgStart &&
         msg.type() != PlatformNotificationMsg_PermissionRequestComplete::ID;
}

}  // namespace content
