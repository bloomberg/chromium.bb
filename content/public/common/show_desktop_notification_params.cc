// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/show_desktop_notification_params.h"

namespace content {

ShowDesktopNotificationHostMsgParams::ShowDesktopNotificationHostMsgParams()
    : is_html(false), direction(WebKit::WebTextDirectionDefault),
      notification_id(0) {
}

ShowDesktopNotificationHostMsgParams::~ShowDesktopNotificationHostMsgParams() {
}

}  // namespace content
