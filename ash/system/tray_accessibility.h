// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#pragma once

#include "ash/system/tray/tray_image_item.h"

namespace views {
class ImageView;
}

namespace ash {

class ASH_EXPORT AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() {}

  virtual void OnAccessibilityModeChanged(bool enabled) = 0;
};

namespace internal {

class TrayAccessibility : public TrayImageItem,
                          public AccessibilityObserver {
 public:
  TrayAccessibility();
  virtual ~TrayAccessibility();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;

  // Overridden from AccessibilityObserver.
  virtual void OnAccessibilityModeChanged(bool enabled) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACCESSIBILITY_H_

