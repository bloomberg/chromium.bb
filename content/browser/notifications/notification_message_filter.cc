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
#include "content/public/browser/platform_notification_service.h"
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
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ShowPersistent,
                        OnShowPersistentNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_Close,
                        OnClosePlatformNotification)
    IPC_MESSAGE_HANDLER(PlatformNotificationHostMsg_ClosePersistent,
                        OnClosePersistentNotification)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message, content::BrowserThread::ID* thread) {
  if (message.type() == PlatformNotificationHostMsg_Show::ID ||
      message.type() == PlatformNotificationHostMsg_ShowPersistent::ID ||
      message.type() == PlatformNotificationHostMsg_Close::ID ||
      message.type() == PlatformNotificationHostMsg_ClosePersistent::ID)
    *thread = BrowserThread::UI;
}

void NotificationMessageFilter::OnCheckNotificationPermission(
    const GURL& origin, blink::WebNotificationPermission* permission) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (service) {
    *permission = service->CheckPermission(resource_context_,
                                           origin,
                                           process_id_);
  } else {
    *permission = blink::WebNotificationPermissionDenied;
  }
}

void NotificationMessageFilter::OnShowPlatformNotification(
    int notification_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  scoped_ptr<DesktopNotificationDelegate> delegate(
      new PageNotificationDelegate(process_id_, notification_id));

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  // TODO(peter): Verify that permission has been granted for |origin|.

  base::Closure close_closure;
  service->DisplayNotification(browser_context_,
                               origin,
                               icon,
                               notification_data,
                               delegate.Pass(),
                               process_id_,
                               &close_closure);

  if (!close_closure.is_null())
    close_closures_[notification_id] = close_closure;
}

void NotificationMessageFilter::OnShowPersistentNotification(
    int64 service_worker_registration_id,
    const GURL& origin,
    const SkBitmap& icon,
    const PlatformNotificationData& notification_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  // TODO(peter): Verify that permission has been granted for |origin|.

  service->DisplayPersistentNotification(browser_context_,
                                         service_worker_registration_id,
                                         origin,
                                         icon,
                                         notification_data,
                                         process_id_);
}

void NotificationMessageFilter::OnClosePlatformNotification(
    int notification_id) {
  if (!close_closures_.count(notification_id))
    return;

  close_closures_[notification_id].Run();
  close_closures_.erase(notification_id);
}

void NotificationMessageFilter::OnClosePersistentNotification(
    const std::string& persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  DCHECK(service);

  service->ClosePersistentNotification(browser_context_,
                                       persistent_notification_id);
}

}  // namespace content
