// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_OBSERVER_H_
#define ASH_SYSTEM_ACCESSIBILITY_OBSERVER_H_

#include "ash/accessibility_types.h"
#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() {}

  // Notifies when accessibility status changes. Used to:
  // * Show the accessibility menu when any feature is enabled.
  // * Show notifications when spoken feedback or braille is enabled.
  // TODO(jamescook): Split into two methods.
  virtual void OnAccessibilityStatusChanged(
      AccessibilityNotificationVisibility notify) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_OBSERVER_H_
