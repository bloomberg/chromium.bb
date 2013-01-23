// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/notifications/notification_types.h"

// Representation of a notification to be shown to the user.
// On non-Ash platforms these are rendered as HTML, sometimes described by a
// data url converted from text + icon data. On Ash they are rendered as
// formated text and icon data.
class Notification {
 public:
  // Initializes a notification with HTML content.
  Notification(const GURL& origin_url,
               const GURL& content_url,
               const string16& display_source,
               const string16& replace_id,
               NotificationDelegate* delegate);

  // Initializes a notification with text content. On non-ash platforms, this
  // creates an HTML representation using a data: URL for display.
  Notification(const GURL& origin_url,
               const GURL& icon_url,
               const string16& title,
               const string16& body,
               WebKit::WebTextDirection dir,
               const string16& display_source,
               const string16& replace_id,
               NotificationDelegate* delegate);

  // Initializes a notification with a given type. Takes ownership of
  // optional_fields.
  Notification(ui::notifications::NotificationType type,
               const GURL& origin_url,
               const GURL& icon_url,
               const string16& title,
               const string16& body,
               WebKit::WebTextDirection dir,
               const string16& display_source,
               const string16& replace_id,
               const DictionaryValue* optional_fields,
               NotificationDelegate* delegate);

  // Initializes a notification with text content and an icon image. Currently
  // only used on Ash. Does not generate content_url_.
  Notification(const GURL& origin_url,
               const gfx::ImageSkia& icon,
               const string16& title,
               const string16& body,
               WebKit::WebTextDirection dir,
               const string16& display_source,
               const string16& replace_id,
               NotificationDelegate* delegate);

  Notification(const Notification& notification);
  ~Notification();
  Notification& operator=(const Notification& notification);

  // If this is a HTML notification.
  bool is_html() const { return is_html_; }

  ui::notifications::NotificationType type() const {
    return type_;
  }

  // The URL (may be data:) containing the contents for the notification.
  const GURL& content_url() const { return content_url_; }

  // Title and message text of the notification.
  const string16& title() const { return title_; }
  const string16& body() const { return body_; }

  // The origin URL of the script which requested the notification.
  const GURL& origin_url() const { return origin_url_; }

  // A url for the icon to be shown (optional).
  const GURL& icon_url() const { return icon_url_; }

  // An image for the icon to be shown (optional).
  const gfx::ImageSkia& icon() const { return icon_; }

  // A display string for the source of the notification.
  const string16& display_source() const { return display_source_; }

  // A unique identifier used to update (replace) or remove a notification.
  const string16& replace_id() const { return replace_id_; }

  const DictionaryValue* optional_fields() const {
    return optional_fields_.get();
  }

  void Display() const { delegate()->Display(); }
  void Error() const { delegate()->Error(); }
  void Click() const { delegate()->Click(); }
  void ButtonClick(int index) const { delegate()->ButtonClick(index); }
  void Close(bool by_user) const { delegate()->Close(by_user); }

  std::string notification_id() const { return delegate()->id(); }

  content::RenderViewHost* GetRenderViewHost() const {
    return delegate()->GetRenderViewHost();
  }

 private:
  NotificationDelegate* delegate() const { return delegate_.get(); }

  // The type of notification we'd like displayed.
  ui::notifications::NotificationType type_;

  // The Origin of the page/worker which created this notification.
  GURL origin_url_;

  // Image data for the associated icon, used by Ash when available.
  gfx::ImageSkia icon_;

  // URL for the icon associated with the notification. Requires delegate_
  // to have a non NULL RenderViewHost.
  GURL icon_url_;

  // If this is a HTML notification, the content is in |content_url_|. If
  // false, the data is in |title_| and |body_|.
  bool is_html_;

  // The URL of the HTML content of the toast (may be a data: URL for simple
  // string-based notifications).
  GURL content_url_;

  // The content for a text notification.
  string16 title_;
  string16 body_;

  // The display string for the source of the notification.  Could be
  // the same as origin_url_, or the name of an extension.
  string16 display_source_;

  // The replace ID for the notification.
  string16 replace_id_;

  scoped_ptr<DictionaryValue> optional_fields_;

  // A proxy object that allows access back to the JavaScript object that
  // represents the notification, for firing events.
  scoped_refptr<NotificationDelegate> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_H_
