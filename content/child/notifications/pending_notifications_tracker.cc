// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/pending_notifications_tracker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/notifications/pending_notification.h"
#include "content/public/common/notification_resources.h"

namespace content {

PendingNotificationsTracker::PendingNotificationsTracker(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : next_notification_id_(0), main_task_runner_(main_task_runner) {}

PendingNotificationsTracker::~PendingNotificationsTracker() {}

void PendingNotificationsTracker::FetchResources(
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    const ResourcesCallback& resources_callback) {
  int32_t notification_id = next_notification_id_++;

  if (delegate)
    delegate_to_pending_id_map_[delegate] = notification_id;

  base::Closure fetches_finished_callback = base::Bind(
      &PendingNotificationsTracker::FetchesFinished,
      base::Unretained(this) /* The pending notifications are owned by this. */,
      delegate, notification_id, resources_callback);

  scoped_ptr<PendingNotification> pending_notification(
      new PendingNotification(main_task_runner_));
  pending_notification->FetchResources(notification_data,
                                       fetches_finished_callback);

  pending_notifications_.AddWithID(pending_notification.release(),
                                   notification_id);
}

bool PendingNotificationsTracker::CancelResourceFetches(
    blink::WebNotificationDelegate* delegate) {
  DCHECK(delegate);

  auto iter = delegate_to_pending_id_map_.find(delegate);
  if (iter == delegate_to_pending_id_map_.end())
    return false;

  pending_notifications_.Remove(iter->second);
  delegate_to_pending_id_map_.erase(iter);

  return true;
}

void PendingNotificationsTracker::FetchesFinished(
    blink::WebNotificationDelegate* delegate,
    int32_t notification_id,
    const ResourcesCallback& resources_callback) {
  PendingNotification* pending_notification =
      pending_notifications_.Lookup(notification_id);
  DCHECK(pending_notification);

  resources_callback.Run(pending_notification->GetResources());

  if (delegate)
    delegate_to_pending_id_map_.erase(delegate);
  pending_notifications_.Remove(notification_id);
}

}  // namespace content
