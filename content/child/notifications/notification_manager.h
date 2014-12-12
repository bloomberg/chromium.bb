// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <map>
#include <set>

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/notification_image_loader.h"
#include "content/child/worker_task_runner.h"
#include "third_party/WebKit/public/platform/WebNotificationManager.h"

class SkBitmap;

namespace blink {
class WebURL;
}

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

  // Asynchronously starts loading |image_url| on the main thread and returns a
  // reference to the notification image loaded responsible for this. |callback|
  // will be invoked on the calling thread when the load is complete.
  scoped_refptr<NotificationImageLoader> CreateImageLoader(
      const blink::WebURL& image_url,
      const NotificationImageLoadedCallback& callback) const;

  // Sends an IPC to the browser process to display the notification,
  // accompanied by the downloaded icon.
  void DisplayNotification(const blink::WebSerializedOrigin& origin,
                           const blink::WebNotificationData& notification_data,
                           blink::WebNotificationDelegate* delegate,
                           scoped_refptr<NotificationImageLoader> image_loader);

  // Sends an IPC to the browser process to display the persistent notification,
  // accompanied by the downloaded icon.
  void DisplayPersistentNotification(
      const blink::WebSerializedOrigin& origin,
      const blink::WebNotificationData& notification_data,
      int64 service_worker_registration_id,
      int request_id,
      scoped_refptr<NotificationImageLoader> image_loader);

  // Removes the notification identified by |delegate| from the set of
  // pending notifications, and returns whether it could be found.
  bool RemovePendingPageNotification(blink::WebNotificationDelegate* delegate);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // Tracking display requests for persistent notifications.
  IDMap<blink::WebNotificationShowCallbacks, IDMapOwnPointer>
      persistent_notification_requests_;

  // A map tracking Page-bound notifications whose icon is still being
  // downloaded. These downloads can be cancelled by the developer.
  std::map<blink::WebNotificationDelegate*,
           scoped_refptr<NotificationImageLoader>>
      pending_page_notifications_;

  // A set tracking Persistent notifications whose icon is still being
  // downloaded. These downloads cannot be cancelled by the developer.
  std::set<scoped_refptr<NotificationImageLoader>>
      pending_persistent_notifications_;

  // Map to store the delegate associated with a notification request Id.
  std::map<int, blink::WebNotificationDelegate*> active_notifications_;

  base::WeakPtrFactory<NotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
