// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification.h"

namespace ash {

namespace message_center {

WebNotification::WebNotification() : is_read(false),
                                     shown_as_popup(false) {
}

WebNotification::~WebNotification() {
}

}  // namespace message_center

}  // namespace ash
