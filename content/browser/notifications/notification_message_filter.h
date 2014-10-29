// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_

#include <map>

#include "base/callback_forward.h"
#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"

class GURL;

namespace content {

class BrowserContext;
class ResourceContext;
struct ShowDesktopNotificationHostMsgParams;

class NotificationMessageFilter : public BrowserMessageFilter {
 public:
  NotificationMessageFilter(
      int process_id,
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
      int notification_id, const ShowDesktopNotificationHostMsgParams& params);
  void OnClosePlatformNotification(int notification_id);

  int process_id_;
  ResourceContext* resource_context_;
  BrowserContext* browser_context_;

  // Map mapping notification ids to their associated close closures.
  std::map<int, base::Closure> close_closures_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_NOTIFICATION_MESSAGE_FILTER_H_
