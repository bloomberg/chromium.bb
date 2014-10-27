// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_NOTIFICATION_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"

class GURL;

namespace content {

class ResourceContext;
struct ShowDesktopNotificationHostMsgParams;

class NotificationMessageFilter : public BrowserMessageFilter {
 public:
  NotificationMessageFilter(
      int process_id,
      ResourceContext* resource_context);

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATION_MESSAGE_FILTER_H_
