// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_

#include <map>

#include "base/callback_forward.h"
#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationPermission.h"

class GURL;
class SkBitmap;

namespace content {

class BrowserContext;
class PlatformNotificationContext;
struct PlatformNotificationData;
class PlatformNotificationService;
class ResourceContext;

class NotificationMessageFilter : public BrowserMessageFilter {
 public:
  NotificationMessageFilter(
      int process_id,
      PlatformNotificationContext* notification_context,
      ResourceContext* resource_context,
      BrowserContext* browser_context);

  // To be called by the notification's delegate when it has closed, so that
  // the close closure associated with that notification can be removed.
  void DidCloseNotification(int notification_id);

  // BrowserMessageFilter implementation. Called on the UI thread.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(
      const IPC::Message& message, content::BrowserThread::ID* thread) override;

 protected:
  ~NotificationMessageFilter() override;

 private:
  void OnCheckNotificationPermission(
      const GURL& origin, blink::WebNotificationPermission* permission);
  void OnShowPlatformNotification(
      int notification_id,
      const GURL& origin,
      const SkBitmap& icon,
      const PlatformNotificationData& notification_data);
  void OnShowPersistentNotification(
      int64 service_worker_registration_id,
      const GURL& origin,
      const SkBitmap& icon,
      const PlatformNotificationData& notification_data);
  void OnClosePlatformNotification(int notification_id);
  void OnClosePersistentNotification(
      const GURL& origin,
      const std::string& persistent_notification_id);

  // Verifies that Web Notification permission has been granted for |origin| in
  // cases where the renderer shouldn't send messages if it weren't the case. If
  // no permission has been granted, a bad message has been received and the
  // renderer should be killed accordingly.
  bool VerifyNotificationPermissionGranted(
      PlatformNotificationService* service,
      const GURL& origin);

  int process_id_;
  scoped_refptr<PlatformNotificationContext> notification_context_;
  ResourceContext* resource_context_;
  BrowserContext* browser_context_;

  // Map mapping notification ids to their associated close closures.
  std::map<int, base::Closure> close_closures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
