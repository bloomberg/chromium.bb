// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification.h"

Notification::Notification(const GURL& origin_url,
                           const GURL& content_url,
                           const string16& display_source,
                           const string16& replace_id,
                           NotificationDelegate* delegate)
    : origin_url_(origin_url),
      content_url_(content_url),
      display_source_(display_source),
      replace_id_(replace_id),
      delegate_(delegate) {
}

Notification::Notification(const Notification& notification)
    : origin_url_(notification.origin_url()),
      content_url_(notification.content_url()),
      display_source_(notification.display_source()),
      replace_id_(notification.replace_id()),
      delegate_(notification.delegate()) {
}

Notification::~Notification() {}

Notification& Notification::operator=(const Notification& notification) {
  origin_url_ = notification.origin_url();
  content_url_ = notification.content_url();
  display_source_ = notification.display_source();
  replace_id_ = notification.replace_id();
  delegate_ = notification.delegate();
  return *this;
}
