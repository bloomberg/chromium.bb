// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notification_message_filter.h"

#include "content/common/platform_notification_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

namespace content {

NotificationMessageFilter::NotificationMessageFilter(
    int process_id,
    ResourceContext* resource_context)
    : BrowserMessageFilter(PlatformNotificationMsgStart),
      process_id_(process_id),
      resource_context_(resource_context) {}

NotificationMessageFilter::~NotificationMessageFilter() {}

bool NotificationMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationMessageFilter, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_CheckPermission,
                        OnCheckNotificationPermission)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Show,
                        OnShowPlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Close,
                        OnClosePlatformNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PlatformNotificationHostMsg_Show::ID ||
      message.type() == PlatformNotificationHostMsg_Close::ID)
    *thread = BrowserThread::UI;
}

void NotificationMessageFilter::OnCheckNotificationPermission(
    const GURL& origin, blink::WebNotificationPermission* permission) {
  *permission =
      GetContentClient()->browser()->CheckDesktopNotificationPermission(
          origin,
          resource_context_,
          process_id_);
}

void NotificationMessageFilter::OnShowPlatformNotification(
    int notification_id, const ShowDesktopNotificationHostMsgParams& params) {
  // TODO(peter): Implement the DesktopNotificationDelegate.
  NOTIMPLEMENTED();
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  // TODO(peter): Implement the ability to close notifications.
  NOTIMPLEMENTED();
}

}  // namespace content
