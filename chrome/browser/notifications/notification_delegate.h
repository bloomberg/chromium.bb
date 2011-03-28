// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"

// Delegate for a notification. This class has two role, to implement
// callback methods for notification, and provides an identify of
// the associated notification.
class NotificationDelegate
    : public base::RefCountedThreadSafe<NotificationDelegate> {
 public:
  // To be called when the desktop notification is actually shown.
  virtual void Display() = 0;

  // To be called when the desktop notification cannot be shown due to an
  // error.
  virtual void Error() = 0;

  // To be called when the desktop notification is closed.  If closed by a
  // user explicitly (as opposed to timeout/script), |by_user| should be true.
  virtual void Close(bool by_user) = 0;

  // To be called when a desktop notification is clicked.
  virtual void Click() = 0;

  // Returns unique id of the notification.
  virtual std::string id() const = 0;

 protected:
  virtual ~NotificationDelegate() {}

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
