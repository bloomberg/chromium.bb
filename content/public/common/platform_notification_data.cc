// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/platform_notification_data.h"

namespace content {

PlatformNotificationData::PlatformNotificationData()
    : direction(NotificationDirectionLeftToRight),
      silent(false) {}

PlatformNotificationData::~PlatformNotificationData() {}

}  // namespace content
