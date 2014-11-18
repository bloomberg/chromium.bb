// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebNotificationManager.h"

class SkBitmap;

namespace content {

class NotificationImageLoader;
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
  virtual void close(blink::WebNotificationDelegate* delegate);
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
  void OnShow(int id);
  void OnClose(int id);
  void OnClick(int id);

  // Called when |pending_notification| has started loading on the main thread.
  void DidStartImageLoad(
      const scoped_refptr<NotificationImageLoader>& pending_notification);

  // Sends an IPC to the browser process to display the notification,
  // accompanied by the downloaded icon.
  void DisplayNotification(const blink::WebSerializedOrigin& origin,
                           const blink::WebNotificationData& notification_data,
                           blink::WebNotificationDelegate* delegate,
                           const SkBitmap& icon);

  // Removes the notification identified by |delegate| from the set of
  // pending notifications, and returns whether it could be found.
  bool RemovePendingNotification(blink::WebNotificationDelegate* delegate);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // A set tracking notifications whose icon is still being downloaded.
  std::set<scoped_refptr<NotificationImageLoader>> pending_notifications_;

  // Map to store the delegate associated with a notification request Id.
  std::map<int, blink::WebNotificationDelegate*> active_notifications_;

  base::WeakPtrFactory<NotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
