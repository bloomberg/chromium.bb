// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebNotificationManager.h"

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
      NotificationDispatcher* notification_dispatcher);

  // IPC message handlers.
  void OnShow(int id);
  void OnClose(int id);
  void OnClick(int id);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // Map to store the delegate associated with a notification request Id.
  std::map<int, blink::WebNotificationDelegate*> active_notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
