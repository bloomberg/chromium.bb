// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

bool DesktopNotificationService::ShowDesktopNotification(
    const GURL& url,
    const GURL&,
    int process_id,
    int route_id,
    NotificationSource source,
    int notification_id) {
  // TODO(johnnyg): http://crbug.com/23954  Linux support coming soon.
  return false;
}

bool DesktopNotificationService::ShowDesktopNotificationText(
    const GURL& origin,
    const GURL& url,
    const string16& title,
    const string16& text,
    int process_id,
    int route_id,
    NotificationSource source,
    int notification_id) {
  // TODO(johnnyg): http://crbug.com/23066  Linux support coming soon.
  // Coming soon.
  return false;
}
