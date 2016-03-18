// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_NOTIFICATION_CONSTANTS_H_
#define CONTENT_COMMON_NOTIFICATION_CONSTANTS_H_

#include <stddef.h>

namespace content {

// Maximum number of actions on a Platform Notification.
static const size_t kPlatformNotificationMaxActions = 2;

// The maximum reasonable notification icon size, scaled from dip units to
// pixels using the largest supported scaling factor.
static const int kPlatformNotificationMaxIconSizePx = 320;  // 80 dip * 4

// The maximum reasonable badge size, scaled from dip units to pixels using the
// largest supported scaling factor.
static const int kPlatformNotificationMaxBadgeSizePx = 96;  // 24 dip * 4

// The maximum reasonable action icon size, scaled from dip units to
// pixels using the largest supported scaling factor.
static const int kPlatformNotificationMaxActionIconSizePx = 128;  // 32 dip * 4

}  // namespace content

#endif  // CONTENT_COMMON_NOTIFICATION_CONSTANTS_H_
