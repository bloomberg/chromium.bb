// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "url/gurl.h"

namespace gfx {
class Image;
}

// Representation of a notification to be shown to the user.
class Notification : public message_center::Notification {
 public:
  Notification(const GURL& origin_url,
               const base::string16& title,
               const base::string16& body,
               const gfx::Image& icon,
               const base::string16& display_source,
               const base::string16& replace_id,
               NotificationDelegate* delegate);

  Notification(
      message_center::NotificationType type,
      const GURL& origin_url,
      const base::string16& title,
      const base::string16& body,
      const gfx::Image& icon,
      blink::WebTextDirection dir,
      const message_center::NotifierId& notifier_id,
      const base::string16& display_source,
      const base::string16& replace_id,
      const message_center::RichNotificationData& rich_notification_data,
      NotificationDelegate* delegate);

  Notification(const std::string& id, const Notification& notification);

  Notification(const Notification& notification);
  virtual ~Notification();
  Notification& operator=(const Notification& notification);

  // The origin URL of the script which requested the notification.
  const GURL& origin_url() const { return origin_url_; }

  // A unique identifier used to update (replace) or remove a notification.
  const base::string16& replace_id() const { return replace_id_; }

  // A url for the button icons to be shown (optional).
  const GURL& button_one_icon_url() const { return button_one_icon_url_; }
  const GURL& button_two_icon_url() const { return button_two_icon_url_; }

  // A url for the image to be shown (optional).
  const GURL& image_url() const { return image_url_; }

  // Id of the delegate embedded inside this instance.
  std::string delegate_id() const { return delegate()->id(); }

  NotificationDelegate* delegate() const { return delegate_.get(); }

 private:
  // The Origin of the page/worker which created this notification.
  GURL origin_url_;

  // The URLs of the button images for a rich notification.
  GURL button_one_icon_url_;
  GURL button_two_icon_url_;

  // The URL of a large image to be displayed for a a rich notification.
  GURL image_url_;

  // The user-supplied replace ID for the notification.
  base::string16 replace_id_;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
