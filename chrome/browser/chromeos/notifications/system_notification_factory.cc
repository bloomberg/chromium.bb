// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/system_notification_factory.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "grit/browser_resources.h"
#include "net/base/escape.h"

namespace chromeos {

using WebKit::WebTextDirection;

// static
Notification SystemNotificationFactory::Create(
    const GURL& icon, const string16& title,
    const string16& text,
    NotificationDelegate* delegate) {
  string16 content_url = DesktopNotificationService::CreateDataUrl(
      icon, title, text, WebKit::WebTextDirectionDefault);
  return Notification(GURL(), GURL(content_url), string16(), string16(),
                      delegate);
}

// static
Notification SystemNotificationFactory::Create(
    const GURL& icon, const string16& title,
    const string16& text, const string16& link,
    NotificationDelegate* delegate) {
  // Create an icon notification with or without a footer link
  // See code in DesktopNotificationService::CreateDataUrl
  WebTextDirection dir = WebKit::WebTextDirectionDefault;
  std::vector<std::string> subst;
  int resource = IDR_NOTIFICATION_ICON_HTML;
  subst.push_back(icon.spec());
  subst.push_back(EscapeForHTML(UTF16ToUTF8(title)));
  subst.push_back(EscapeForHTML(UTF16ToUTF8(text)));
  // icon float position
  subst.push_back(dir == WebKit::WebTextDirectionRightToLeft ?
                  "right" : "left");
  // body text direction
  subst.push_back(dir == WebKit::WebTextDirectionRightToLeft ?
                  "rtl" : "ltr");
  // if link is not empty, then use template with link
  if (!link.empty()) {
    resource = IDR_NOTIFICATION_ICON_LINK_HTML;
    subst.push_back(EscapeForHTML(UTF16ToUTF8(link)));
  }

  string16 content_url = DesktopNotificationService::CreateDataUrl(resource,
                                                                   subst);
  return Notification(GURL(), GURL(content_url), string16(), string16(),
                      delegate);
}

}  // namespace chromeos
