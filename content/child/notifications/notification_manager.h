// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <vector>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/pending_notifications_tracker.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/child/worker_thread.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationManager.h"

class SkBitmap;

namespace content {

struct PlatformNotificationData;
class ThreadSafeSender;

class NotificationManager : public blink::WebNotificationManager,
                            public WorkerThread::Observer {
 public:
  ~NotificationManager() override;

  // |thread_safe_sender| and |notification_dispatcher| are used if
  // calling this leads to construction.
  static NotificationManager* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner,
      NotificationDispatcher* notification_dispatcher);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  // blink::WebNotificationManager implementation.
  void show(const blink::WebSecurityOrigin& origin,
            const blink::WebNotificationData& notification_data,
            blink::WebNotificationDelegate* delegate) override;
  void showPersistent(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebNotificationShowCallbacks* callbacks) override;
  void getNotifications(
      const blink::WebString& filter_tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebNotificationGetCallbacks* callbacks) override;
  void close(blink::WebNotificationDelegate* delegate) override;
  void closePersistent(const blink::WebSecurityOrigin& origin,
                       int64_t persistent_notification_id) override;
  void notifyDelegateDestroyed(
      blink::WebNotificationDelegate* delegate) override;
  blink::WebNotificationPermission checkPermission(
      const blink::WebSecurityOrigin& origin) override;
  size_t maxActions() override;

  // Called by the NotificationDispatcher.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  NotificationManager(ThreadSafeSender* thread_safe_sender,
                      base::SingleThreadTaskRunner* main_thread_task_runner,
                      NotificationDispatcher* notification_dispatcher);

  // IPC message handlers.
  void OnDidShow(int notification_id);
  void OnDidShowPersistent(int request_id, bool success);
  void OnDidClose(int notification_id);
  void OnDidClick(int notification_id);
  void OnDidGetNotifications(
      int request_id,
      const std::vector<PersistentNotificationInfo>& notification_infos);

  // To be called when a page notification is ready to be displayed. Will
  // inform the browser process about all available data. The |delegate|,
  // owned by Blink, will be used to feed back events associated with the
  // notification to the JavaScript object.
  void DisplayPageNotification(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      blink::WebNotificationDelegate* delegate,
      const SkBitmap& icon);

  // To be called when a persistent notification is ready to be displayed. Will
  // inform the browser process about all available data. The |callbacks| will
  // be used to inform the Promise pending in Blink that the notification has
  // been send to the browser process to be displayed.
  void DisplayPersistentNotification(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      int64_t service_worker_registration_id,
      scoped_ptr<blink::WebNotificationShowCallbacks> callbacks,
      const SkBitmap& icon);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // Tracker which stores all pending Notifications, both page and persistent
  // ones, until all their associated resources have been fetched.
  PendingNotificationsTracker pending_notifications_;

  // Tracks pending requests for getting a list of notifications.
  IDMap<blink::WebNotificationGetCallbacks, IDMapOwnPointer>
      pending_get_notification_requests_;

  // Tracks pending requests for displaying persistent notifications.
  IDMap<blink::WebNotificationShowCallbacks, IDMapOwnPointer>
      pending_show_notification_requests_;

  // Map to store the delegate associated with a notification request Id.
  std::map<int, blink::WebNotificationDelegate*> active_page_notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
