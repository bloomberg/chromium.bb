// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/pending_notifications_tracker.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationManager.h"

class SkBitmap;

namespace content {

class ThreadSafeSender;

class NotificationManager : public blink::WebNotificationManager,
                            public WorkerTaskRunner::Observer {
 public:
  ~NotificationManager() override;

  // |thread_safe_sender| and |notification_dispatcher| are used if
  // calling this leads to construction.
  static NotificationManager* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner,
      NotificationDispatcher* notification_dispatcher);

  // WorkerTaskRunner::Observer implementation.
  void OnWorkerRunLoopStopped() override;

  // blink::WebNotificationManager implementation.
  virtual void show(const blink::WebSerializedOrigin& origin,
                    const blink::WebNotificationData& notification_data,
                    blink::WebNotificationDelegate* delegate);
  virtual void showPersistent(
      const blink::WebSerializedOrigin& origin,
      const blink::WebNotificationData& notification_data,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebNotificationShowCallbacks* callbacks);
  virtual void close(blink::WebNotificationDelegate* delegate);
  virtual void closePersistent(
      const blink::WebSerializedOrigin& origin,
      const blink::WebString& persistent_notification_id);
  virtual void notifyDelegateDestroyed(
      blink::WebNotificationDelegate* delegate);
  virtual blink::WebNotificationPermission checkPermission(
      const blink::WebSerializedOrigin& origin);

  // Called by the NotificationDispatcher.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  NotificationManager(
      ThreadSafeSender* thread_safe_sender,
      base::SingleThreadTaskRunner* main_thread_task_runner,
      NotificationDispatcher* notification_dispatcher);

  // IPC message handlers.
  void OnDidShow(int notification_id);
  void OnDidClose(int notification_id);
  void OnDidClick(int notification_id);

  // To be called when a page notification is ready to be displayed. Will
  // inform the browser process about all available data. The |delegate|,
  // owned by Blink, will be used to feed back events associated with the
  // notification to the JavaScript object.
  void DisplayPageNotification(
      const blink::WebSerializedOrigin& origin,
      const blink::WebNotificationData& notification_data,
      blink::WebNotificationDelegate* delegate,
      const SkBitmap& icon);

  // To be called when a persistent notification is ready to be displayed. Will
  // inform the browser process about all available data. The |callbacks| will
  // be used to inform the Promise pending in Blink that the notification has
  // been send to the browser process to be displayed.
  void DisplayPersistentNotification(
      const blink::WebSerializedOrigin& origin,
      const blink::WebNotificationData& notification_data,
      int64 service_worker_registration_id,
      scoped_ptr<blink::WebNotificationShowCallbacks> callbacks,
      const SkBitmap& icon);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // Tracker which stores all pending Notifications, both page and persistent
  // ones, until all their associated resources have been fetched.
  PendingNotificationsTracker pending_notifications_;

  // Map to store the delegate associated with a notification request Id.
  std::map<int, blink::WebNotificationDelegate*> active_page_notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
