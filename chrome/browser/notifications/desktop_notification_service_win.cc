// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/logging.h"
#include "base/win/metro.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "win8/util/win8_util.h"

typedef void (*MetroDisplayNotification)(
    const char* origin_url, const char* icon_url, const wchar_t* title,
    const wchar_t* body, const wchar_t* display_source,
    const char* notification_id);

typedef bool (*MetroCancelNotification)(const char* notification_id);

bool DesktopNotificationService::CancelDesktopNotification(
    int process_id, int route_id, int notification_id) {
  scoped_refptr<NotificationObjectProxy> proxy(
      new NotificationObjectProxy(process_id, route_id, notification_id,
                                  false));
  if (win8::IsSingleWindowMetroMode()) {
    MetroCancelNotification cancel_metro_notification =
        reinterpret_cast<MetroCancelNotification>(GetProcAddress(
            base::win::GetMetroModule(), "CancelNotification"));
    DCHECK(cancel_metro_notification);
    if (cancel_metro_notification(proxy->id().c_str()))
      return true;
  }
  return GetUIManager()->CancelById(proxy->id());
}

void DesktopNotificationService::ShowNotification(
    const Notification& notification) {
  if (win8::IsSingleWindowMetroMode()) {
    MetroDisplayNotification display_metro_notification =
        reinterpret_cast<MetroDisplayNotification>(GetProcAddress(
            base::win::GetMetroModule(), "DisplayNotification"));
    DCHECK(display_metro_notification);
    if (!notification.is_html()) {
      display_metro_notification(notification.origin_url().spec().c_str(),
                                 notification.content_url().spec().c_str(),
                                 notification.title().c_str(),
                                 notification.body().c_str(),
                                 notification.display_source().c_str(),
                                 notification.notification_id().c_str());
      return;
    } else {
      NOTREACHED() << "We don't support HTML notifications in Windows 8 metro";
    }
  }
  GetUIManager()->Add(notification, profile_);
}
