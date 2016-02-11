// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_
#define CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
struct WebNotificationData;
class WebNotificationDelegate;
}

namespace content {

class PendingNotification;
class PendingNotificationsTrackerTest;
struct NotificationResources;

// Tracks all pending Web Notifications as PendingNotification instances. The
// pending notifications (and their associated data) are stored in this class.
// The pending notification tracker is owned by the NotificationManager, and
// lives on the thread that manager has been associated with.
class CONTENT_EXPORT PendingNotificationsTracker {
 public:
  explicit PendingNotificationsTracker(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner);
  ~PendingNotificationsTracker();

  // Type definition for the callback signature which is to be invoked when the
  // resources associated with a notification have been fetched.
  using ResourcesCallback = base::Callback<void(const NotificationResources&)>;

  // Adds a pending notification for which to fetch resources. The |delegate|
  // may be null, but non-null values can be used to cancel the fetches.
  // Resource fetches will be started asynchronously on the main thread.
  void FetchResources(const blink::WebNotificationData& notification_data,
                      blink::WebNotificationDelegate* delegate,
                      const ResourcesCallback& resources_callback);

  // Cancels all pending and in-flight fetches for the notification identified
  // by |delegate|, which may not be null. Returns whether the notification was
  // cancelled.
  bool CancelResourceFetches(blink::WebNotificationDelegate* delegate);

 private:
  friend class PendingNotificationsTrackerTest;

  // To be called on the worker thread when the pending notification
  // identified by |notification_id| has finished fetching the resources. The
  // |delegate| may be null.
  void FetchesFinished(blink::WebNotificationDelegate* delegate,
                       int notification_id,
                       const ResourcesCallback& resources_callback);

  // Used to generate ids for tracking pending notifications.
  int32_t next_notification_id_;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // The notifications whose resources are still being fetched.
  IDMap<PendingNotification, IDMapOwnPointer> pending_notifications_;

  // In order to be able to cancel pending page notifications by delegate, store
  // a mapping of the delegate to the pending notification id as well.
  std::map<blink::WebNotificationDelegate*, int> delegate_to_pending_id_map_;

  DISALLOW_COPY_AND_ASSIGN(PendingNotificationsTracker);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_
