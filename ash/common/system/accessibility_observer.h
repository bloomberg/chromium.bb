// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_ACCESSIBILITY_OBSERVER_H_
#define ASH_COMMON_SYSTEM_ACCESSIBILITY_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/common/accessibility_types.h"

namespace ash {

class ASH_EXPORT AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() {}

  // Notifies when accessibility mode changes.
  virtual void OnAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_ACCESSIBILITY_OBSERVER_H_
