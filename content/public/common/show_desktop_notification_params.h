// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_
#pragma once

#include "content/common/content_export.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"

namespace content {

// Parameters used when showing an HTML5 notification.
struct CONTENT_EXPORT ShowDesktopNotificationHostMsgParams {
  ShowDesktopNotificationHostMsgParams();
  ~ShowDesktopNotificationHostMsgParams();

  // URL which is the origin that created this notification.
  GURL origin;

  // True if this is HTML
  bool is_html;

  // URL which contains the HTML contents (if is_html is true), otherwise empty.
  GURL contents_url;

  // Contents of the notification if is_html is false.
  GURL icon_url;
  string16 title;
  string16 body;

  // Directionality of the notification.
  WebKit::WebTextDirection direction;

  // ReplaceID if this notification should replace an existing one; may be
  // empty if no replacement is called for.
  string16 replace_id;

  // Notification ID for sending events back for this notification.
  int notification_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_
