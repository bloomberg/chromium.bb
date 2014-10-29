// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/notification_message_filter.h"

#include "base/callback.h"
#include "content/browser/notifications/page_notification_delegate.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/content_client.h"

namespace content {

NotificationMessageFilter::NotificationMessageFilter(
    int process_id,
    ResourceContext* resource_context,
    BrowserContext* browser_context)
    : BrowserMessageFilter(PlatformNotificationMsgStart),
      process_id_(process_id),
      resource_context_(resource_context),
      browser_context_(browser_context) {}

NotificationMessageFilter::~NotificationMessageFilter() {}

void NotificationMessageFilter::DidCloseNotification(int notification_id) {
  close_closures_.erase(notification_id);
}

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
  scoped_ptr<DesktopNotificationDelegate> delegate(
      new PageNotificationDelegate(process_id_, notification_id));

  base::Closure close_closure;
  GetContentClient()->browser()->ShowDesktopNotification(params,
                                                         browser_context_,
                                                         process_id_,
                                                         delegate.Pass(),
                                                         &close_closure);

  if (!close_closure.is_null())
    close_closures_[notification_id] = close_closure;
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  if (!close_closures_.count(notification_id))
    return;

  close_closures_[notification_id].Run();
  close_closures_.erase(notification_id);
}

}  // namespace content
