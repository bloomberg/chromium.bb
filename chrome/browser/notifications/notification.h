// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "googleurl/src/gurl.h"

class NotificationDelegate;

// Representation of an notification to be shown to the user.  All
// notifications at this level are HTML, although they may be
// data: URLs representing simple text+icon notifications.
class Notification {
 public:
  Notification(const GURL& origin_url,
               const GURL& content_url,
               const string16& display_source,
               const string16& replace_id,
               NotificationDelegate* delegate);
  Notification(const Notification& notification);
  ~Notification();
  Notification& operator=(const Notification& notification);

  // The URL (may be data:) containing the contents for the notification.
  const GURL& content_url() const { return content_url_; }

  // The origin URL of the script which requested the notification.
  const GURL& origin_url() const { return origin_url_; }

  // A display string for the source of the notification.
  const string16& display_source() const { return display_source_; }

  const string16& replace_id() const { return replace_id_; }

  void Display() const { delegate()->Display(); }
  void Error() const { delegate()->Error(); }
  void Click() const { delegate()->Click(); }
  void Close(bool by_user) const { delegate()->Close(by_user); }

  std::string notification_id() const { return delegate()->id(); }

 private:
  NotificationDelegate* delegate() const { return delegate_.get(); }

  // The Origin of the page/worker which created this notification.
  GURL origin_url_;

  // The URL of the HTML content of the toast (may be a data: URL for simple
  // string-based notifications).
  GURL content_url_;

  // The display string for the source of the notification.  Could be
  // the same as origin_url_, or the name of an extension.
  string16 display_source_;

  // The replace ID for the notification.
  string16 replace_id_;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
