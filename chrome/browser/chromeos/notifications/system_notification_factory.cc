// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification_factory.h"

#include "chrome/browser/notifications/desktop_notification_service.h"

namespace chromeos {

// static
Notification SystemNotificationFactory::Create(
    const GURL& icon, const string16& title,
    const string16& text,
    NotificationDelegate* delegate) {
  string16 content_url = DesktopNotificationService::CreateDataUrl(
      icon, title, text, WebKit::WebTextDirectionDefault);
  return Notification(GURL(), GURL(content_url), std::wstring(), string16(),
                      delegate);
}
}  // namespace chromeos
