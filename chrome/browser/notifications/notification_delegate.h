// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "ui/message_center/notification_delegate.h"

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
class NotificationDelegate : public message_center::NotificationDelegate {
 public:
  // Returns unique id of the notification.
  virtual std::string id() const = 0;

 protected:
  ~NotificationDelegate() override {}
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
