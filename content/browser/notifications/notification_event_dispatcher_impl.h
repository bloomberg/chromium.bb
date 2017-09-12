// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/notification_event_dispatcher.h"

namespace content {

class NotificationEventDispatcherImpl : public NotificationEventDispatcher {
 public:
  // Returns the instance of the NotificationEventDispatcherImpl. Must be called
  // on the UI thread.
  static NotificationEventDispatcherImpl* GetInstance();

  // NotificationEventDispatcher implementation.
  void DispatchNotificationClickEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const base::Optional<int>& action_index,
      const base::Optional<base::string16>& reply,
      const NotificationDispatchCompleteCallback& dispatch_complete_callback)
      override;
  void DispatchNotificationCloseEvent(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      bool by_user,
      const NotificationDispatchCompleteCallback& dispatch_complete_callback)
      override;
  void DispatchNonPersistentShowEvent(
      const std::string& notification_id) override;
  void DispatchNonPersistentClickEvent(
      const std::string& notification_id) override;
  void DispatchNonPersistentCloseEvent(
      const std::string& notification_id) override;

  // Called when a renderer that had shown a non persistent notification
  // dissappears.
  void RendererGone(int renderer_id);

  // Regsiter the fact that a non persistent notification has been
  // displayed.
  void RegisterNonPersistentNotification(const std::string& notification_id,
                                         int renderer_id,
                                         int non_persistent_id);

 private:
  NotificationEventDispatcherImpl();
  ~NotificationEventDispatcherImpl() override;

  // Notification Id -> renderer Id.
  std::map<std::string, int> renderer_ids_;

  // Notification Id -> non-persistent notification id.
  std::map<std::string, int> non_persistent_ids_;

  friend struct base::DefaultSingletonTraits<NotificationEventDispatcherImpl>;

  DISALLOW_COPY_AND_ASSIGN(NotificationEventDispatcherImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_EVENT_DISPATCHER_IMPL_H_
