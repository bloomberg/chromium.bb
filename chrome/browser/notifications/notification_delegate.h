// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace content {
class RenderViewHost;
}

// Delegate for a notification. This class has two roles: to implement callback
// methods for notification, and to provide an identity of the associated
// notification.
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

  // To be called when the user clicks a button in a notification. TODO(miket):
  // consider providing default implementations of the pure virtuals of this
  // interface, to avoid pinging so many OWNERs each time we enhance it.
  virtual void ButtonClick(int button_index);

  // Returns unique id of the notification.
  virtual std::string id() const = 0;

  // Returns the id of renderer process which creates the notification, or -1.
  virtual int process_id() const;

  // Returns the RenderViewHost that generated the notification, or NULL.
  virtual content::RenderViewHost* GetRenderViewHost() const = 0;

  // Lets the delegate know that no more rendering will be necessary.
  virtual void ReleaseRenderViewHost();

 protected:
  virtual ~NotificationDelegate() {}

 private:
  friend class base::RefCountedThreadSafe<NotificationDelegate>;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DELEGATE_H_
