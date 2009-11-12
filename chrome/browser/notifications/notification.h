// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "googleurl/src/gurl.h"

// Representation of an notification to be shown to the user.  All
// notifications at this level are HTML, although they may be
// data: URLs representing simple text+icon notifications.
class Notification {
 public:
  Notification(const GURL& origin_url, const GURL& content_url,
               const std::wstring& display_source,
               NotificationObjectProxy* proxy)
      : origin_url_(origin_url),
        content_url_(content_url),
        display_source_(display_source),
        proxy_(proxy) {
  }

  Notification(const Notification& notification)
      : origin_url_(notification.origin_url()),
        content_url_(notification.content_url()),
        display_source_(notification.display_source()),
        proxy_(notification.proxy()) {
  }

  // The URL (may be data:) containing the contents for the notification.
  const GURL& content_url() const { return content_url_; }

  // The origin URL of the script which requested the notification.
  const GURL& origin_url() const { return origin_url_; }

  // A display string for the source of the notification.
  const std::wstring& display_source() const { return display_source_; }

  void Display() const { proxy()->Display(); }
  void Error() const { proxy()->Error(); }
  void Close(bool by_user) const { proxy()->Close(by_user); }

  bool IsSame(const Notification& other) const {
    return (*proxy_).IsSame(*(other.proxy()));
  }

 private:
  NotificationObjectProxy* proxy() const { return proxy_.get(); }

  // The Origin of the page/worker which created this notification.
  GURL origin_url_;

  // The URL of the HTML content of the toast (may be a data: URL for simple
  // string-based notifications).
  GURL content_url_;

  // The display string for the source of the notification.  Could be
  // the same as origin_url_, or the name of an extension.
  std::wstring display_source_;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationObjectProxy> proxy_;

  // Disallow assign.  Copy constructor written above.
  void operator=(const Notification&);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
