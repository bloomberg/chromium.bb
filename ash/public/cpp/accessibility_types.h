// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_TYPES_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_TYPES_H_

namespace ash {

enum AccessibilityNotificationVisibility {
  A11Y_NOTIFICATION_NONE,
  A11Y_NOTIFICATION_SHOW,
};

// Note: Do not change these values; UMA and prefs depend on them.
enum MagnifierType {
  MAGNIFIER_DISABLED = 0,  // Used by enterprise policy.
  MAGNIFIER_FULL = 1,
  // Never shipped. Deprioritized in 2013. http://crbug.com/170850
  // MAGNIFIER_PARTIAL = 2,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_TYPES_H_
