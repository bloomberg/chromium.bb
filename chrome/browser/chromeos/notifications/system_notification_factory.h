// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/notifications/notification.h"

class GURL;
class NotificationDelegate;

namespace chromeos {

// A utility class for system notifications.
class SystemNotificationFactory {
 public:

  // Creates a system notification.
  static Notification Create(
      const GURL& icon, const string16& title,
      const string16& text,
      NotificationDelegate* delegate);

  // Creates a system notification with a footer link.
  static Notification Create(
      const GURL& icon, const string16& title,
      const string16& text, const string16& link,
      NotificationDelegate* delegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_SYSTEM_NOTIFICATION_FACTORY_H_
