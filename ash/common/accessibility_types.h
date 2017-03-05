// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_ACCESSIBILITY_TYPES_H_
#define ASH_COMMON_ACCESSIBILITY_TYPES_H_

namespace ash {

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

enum AccessibilityAlert {
  A11Y_ALERT_NONE,
  A11Y_ALERT_CAPS_ON,
  A11Y_ALERT_CAPS_OFF,
  A11Y_ALERT_SCREEN_ON,
  A11Y_ALERT_SCREEN_OFF,
  A11Y_ALERT_WINDOW_NEEDED,
  A11Y_ALERT_WINDOW_OVERVIEW_MODE_ENTERED
};

// Note: Do not change these values; UMA and prefs depend on them.
enum MagnifierType {
  MAGNIFIER_FULL = 1,
  MAGNIFIER_PARTIAL = 2,
};

const int kMaxMagnifierType = 2;

const MagnifierType kDefaultMagnifierType = MAGNIFIER_FULL;

// Factor of magnification scale. For example, when this value is 1.189, scale
// value will be changed x1.000, x1.189, x1.414, x1.681, x2.000, ...
// Note: this value is 2.0 ^ (1 / 4).
const float kMagnificationScaleFactor = 1.18920712f;

}  // namespace ash

#endif  // ASH_COMMON_ACCESSIBILITY_TYPES_H_
