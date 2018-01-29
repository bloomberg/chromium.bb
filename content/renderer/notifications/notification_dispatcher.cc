// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/notifications/notification_dispatcher.h"

#include <limits>

#include "content/renderer/notifications/notification_manager.h"

namespace content {

NotificationDispatcher::NotificationDispatcher(
    ThreadSafeSender* thread_safe_sender)
    : WorkerThreadMessageFilter(thread_safe_sender) {}

NotificationDispatcher::~NotificationDispatcher() {}

int NotificationDispatcher::GenerateNotificationRequestId(int thread_id) {
  base::AutoLock lock(notification_request_id_map_lock_);
  CHECK_LT(next_notification_request_id_, std::numeric_limits<int>::max());

  notification_request_id_map_[next_notification_request_id_] = thread_id;
  return next_notification_request_id_++;
}

bool NotificationDispatcher::ShouldHandleMessage(
    const IPC::Message& msg) const {
  return IPC_MESSAGE_CLASS(msg) == PlatformNotificationMsgStart;
}

void NotificationDispatcher::OnFilteredMessageReceived(
    const IPC::Message& msg) {
  NotificationManager::ThreadSpecificInstance(thread_safe_sender(), this)
      ->OnMessageReceived(msg);
}

bool NotificationDispatcher::GetWorkerThreadIdForMessage(
    const IPC::Message& msg,
    int* ipc_thread_id) {
  int notification_request_id = -1;
  const bool success =
      base::PickleIterator(msg).ReadInt(&notification_request_id);
  DCHECK(success);

  base::AutoLock lock(notification_request_id_map_lock_);
  auto iterator = notification_request_id_map_.find(notification_request_id);
  if (iterator != notification_request_id_map_.end()) {
    *ipc_thread_id = iterator->second;
    return true;
  }
  return false;
}

}  // namespace content
