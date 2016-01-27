// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/pending_notifications_tracker.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "content/child/notifications/notification_image_loader.h"
#include "content/public/common/notification_resources.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationData.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

// Stores the information associated with a pending notification.
struct PendingNotificationsTracker::PendingNotification {
  PendingNotification(
      const scoped_refptr<NotificationImageLoader>& image_loader,
      const NotificationResourcesFetchedCallback& callback)
      : image_loader(image_loader), callback(callback) {}

  scoped_refptr<NotificationImageLoader> image_loader;
  NotificationResourcesFetchedCallback callback;
};

PendingNotificationsTracker::PendingNotificationsTracker(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner), weak_factory_(this) {}

PendingNotificationsTracker::~PendingNotificationsTracker() {}

void PendingNotificationsTracker::FetchPageNotificationResources(
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    const NotificationResourcesFetchedCallback& callback) {
  delegate_to_pending_id_map_[delegate] = FetchNotificationResources(
      notification_data, callback,
      new NotificationImageLoader(
          base::Bind(&PendingNotificationsTracker::DidFetchPageNotification,
                     weak_factory_.GetWeakPtr(), delegate),
          base::ThreadTaskRunnerHandle::Get()));
}

void PendingNotificationsTracker::FetchPersistentNotificationResources(
    const blink::WebNotificationData& notification_data,
    const NotificationResourcesFetchedCallback& callback) {
  FetchNotificationResources(
      notification_data, callback,
      new NotificationImageLoader(
          base::Bind(
              &PendingNotificationsTracker::DidFetchPersistentNotification,
              weak_factory_.GetWeakPtr()),
          base::ThreadTaskRunnerHandle::Get()));
}

bool PendingNotificationsTracker::CancelPageNotificationFetches(
    blink::WebNotificationDelegate* delegate) {
  auto iter = delegate_to_pending_id_map_.find(delegate);
  if (iter == delegate_to_pending_id_map_.end())
    return false;

  pending_notifications_.Remove(iter->second);
  delegate_to_pending_id_map_.erase(iter);

  return true;
}

void PendingNotificationsTracker::DidFetchPageNotification(
    blink::WebNotificationDelegate* delegate,
    int notification_id,
    const SkBitmap& icon) {
  PendingNotification* pending_notification =
      pending_notifications_.Lookup(notification_id);
  DCHECK(pending_notification);

  NotificationResources notification_resources;
  notification_resources.notification_icon = icon;
  pending_notification->callback.Run(notification_resources);

  delegate_to_pending_id_map_.erase(delegate);
  pending_notifications_.Remove(notification_id);
}

void PendingNotificationsTracker::DidFetchPersistentNotification(
    int notification_id, const SkBitmap& icon) {
  PendingNotification* pending_notification =
      pending_notifications_.Lookup(notification_id);
  DCHECK(pending_notification);

  NotificationResources notification_resources;
  notification_resources.notification_icon = icon;
  pending_notification->callback.Run(notification_resources);

  pending_notifications_.Remove(notification_id);
}

int PendingNotificationsTracker::FetchNotificationResources(
    const blink::WebNotificationData& notification_data,
    const NotificationResourcesFetchedCallback& callback,
    const scoped_refptr<NotificationImageLoader>& image_loader) {
  int notification_id = pending_notifications_.Add(
      new PendingNotification(image_loader, callback));

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NotificationImageLoader::StartOnMainThread, image_loader,
                 notification_id, GURL(notification_data.icon)));

  return notification_id;
}

}  // namespace content
