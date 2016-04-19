// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

// Representation of a notification to be shown to the user.
class Notification : public message_center::Notification {
 public:
  Notification(
      message_center::NotificationType type,
      const base::string16& title,
      const base::string16& body,
      const gfx::Image& icon,
      const message_center::NotifierId& notifier_id,
      const base::string16& display_source,
      const GURL& origin_url,
      const std::string& tag,
      const message_center::RichNotificationData& rich_notification_data,
      NotificationDelegate* delegate);

  Notification(const std::string& id, const Notification& notification);

  Notification(const Notification& notification);
  ~Notification() override;
  Notification& operator=(const Notification& notification);

  // A unique identifier used to update (replace) or remove a notification.
  const std::string& tag() const { return tag_; }

  // Id of the delegate embedded inside this instance.
  std::string delegate_id() const { return delegate()->id(); }

  NotificationDelegate* delegate() const { return delegate_.get(); }

 private:
  // The user-supplied tag for the notification.
  std::string tag_;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
