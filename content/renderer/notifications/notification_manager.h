// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
#define CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_MANAGER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/renderer/worker_thread.h"
#include "content/renderer/notifications/notification_dispatcher.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationManager.h"
#include "url/gurl.h"

namespace content {

class ThreadSafeSender;

class NotificationManager : public blink::WebNotificationManager,
                            public WorkerThread::Observer {
 public:
  ~NotificationManager() override;

  // |thread_safe_sender| and |notification_dispatcher| are used if
  // calling this leads to construction.
  static NotificationManager* ThreadSpecificInstance(
      ThreadSafeSender* thread_safe_sender,
      NotificationDispatcher* notification_dispatcher);

  // WorkerThread::Observer implementation.
  void WillStopCurrentWorkerThread() override;

  void Show(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      std::unique_ptr<blink::WebNotificationResources> notification_resources,
      blink::WebNotificationDelegate* delegate) override;
  void ShowPersistent(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      std::unique_ptr<blink::WebNotificationResources> notification_resources,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      std::unique_ptr<blink::WebNotificationShowCallbacks> callbacks) override;
  void GetNotifications(
      const blink::WebString& filter_tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      std::unique_ptr<blink::WebNotificationGetCallbacks> callbacks) override;
  void Close(blink::WebNotificationDelegate* delegate) override;
  void ClosePersistent(const blink::WebSecurityOrigin& origin,
                       const blink::WebString& tag,
                       const blink::WebString& notification_id) override;
  void NotifyDelegateDestroyed(
      blink::WebNotificationDelegate* delegate) override;

  // Called by the NotificationDispatcher.
  bool OnMessageReceived(const IPC::Message& message);

 private:
  NotificationManager(ThreadSafeSender* thread_safe_sender,
                      NotificationDispatcher* notification_dispatcher);

  // IPC message handlers.
  void OnDidShow(int request_id);
  void OnDidShowPersistent(int request_id, bool success);
  void OnDidClose(int request_id);
  void OnDidClick(int request_id);
  void OnDidGetNotifications(
      int request_id,
      const std::vector<PersistentNotificationInfo>& notification_infos);

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<NotificationDispatcher> notification_dispatcher_;

  // Tracks pending requests for getting a list of notifications.
  base::IDMap<std::unique_ptr<blink::WebNotificationGetCallbacks>>
      pending_get_notification_requests_;

  // Tracks pending requests for displaying persistent notifications.
  base::IDMap<std::unique_ptr<blink::WebNotificationShowCallbacks>>
      pending_show_notification_requests_;

  // Structure holding the information for active non-persistent notifications.
  struct ActiveNotificationData {
    ActiveNotificationData() = default;
    ActiveNotificationData(blink::WebNotificationDelegate* delegate,
                           const GURL& origin,
                           const std::string& tag);
    ~ActiveNotificationData();

    blink::WebNotificationDelegate* delegate = nullptr;
    GURL origin;
    std::string tag;
  };

  // Map to store the delegate associated with a notification request Id.
  std::unordered_map<int, ActiveNotificationData> active_page_notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationManager);
};

}  // namespace content

#endif  // CONTENT_RENDERER_NOTIFICATIONS_NOTIFICATION_MANAGER_H_
