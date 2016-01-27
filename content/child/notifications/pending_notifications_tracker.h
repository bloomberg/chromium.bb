// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_
#define CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_

#include <map>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

class SkBitmap;

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
struct WebNotificationData;
class WebNotificationDelegate;
}

namespace content {

class NotificationImageLoader;
struct NotificationResources;

// Type definition for the callback signature which is to be invoked when the
// resources associated with a notification have been fetched.
using NotificationResourcesFetchedCallback =
    base::Callback<void(const NotificationResources&)>;

// Tracks all aspects of all pending Web Notifications. Most notably, it's in
// charge of ensuring that all resource fetches associated with the notification
// are completed as expected. The data associated with the notifications is
// stored in this class, to maintain thread integrity of their members.
//
// The pending notification tracker is owned by the NotificationManager, and
// lives on the thread that manager has been associated with.
class PendingNotificationsTracker {
 public:
  explicit PendingNotificationsTracker(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~PendingNotificationsTracker();

  // Adds a page notification to the tracker. Resource fetches for the
  // notification will be started on asynchronously the main thread.
  void FetchPageNotificationResources(
      const blink::WebNotificationData& notification_data,
      blink::WebNotificationDelegate* delegate,
      const NotificationResourcesFetchedCallback& callback);

  // Adds a persistent notification to the tracker. Resource fetches for the
  // notification will be started asynchronously on the main thread.
  void FetchPersistentNotificationResources(
      const blink::WebNotificationData& notification_data,
      const NotificationResourcesFetchedCallback& callback);

  // Cancels all pending and in-fligth fetches for the page notification
  // identified by |delegate|. Returns if the notification was cancelled.
  bool CancelPageNotificationFetches(blink::WebNotificationDelegate* delegate);

 private:
  // To be called on the worker thread when the pending page notification
  // identified by |notification_id| has finished fetching the icon.
  void DidFetchPageNotification(blink::WebNotificationDelegate* delegate,
                                int notification_id,
                                const SkBitmap& icon);

  // To be called on the worker thread when the pending persistent notification
  // identified by |notification_id| has finished fetching the icon.
  void DidFetchPersistentNotification(int notification_id,
                                      const SkBitmap& icon);

  // Common code for starting to fetch resources associated with any kind of
  // notification. Will return the id of the pending notification as allocated
  // in the |pending_notifications_| map.
  int FetchNotificationResources(
      const blink::WebNotificationData& notification_data,
      const NotificationResourcesFetchedCallback& callback,
      const scoped_refptr<NotificationImageLoader>& image_loader);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  struct PendingNotification;

  // List of the notifications whose resources are still being fetched.
  IDMap<PendingNotification, IDMapOwnPointer> pending_notifications_;

  // In order to be able to cancel pending page notifications by delegate, store
  // a mapping of the delegate to the pending notification id as well.
  std::map<blink::WebNotificationDelegate*, int> delegate_to_pending_id_map_;

  base::WeakPtrFactory<PendingNotificationsTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingNotificationsTracker);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_PENDING_NOTIFICATIONS_TRACKER_H_
