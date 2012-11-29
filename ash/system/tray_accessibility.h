// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_TRAY_ACCESSIBILITY_H_

#include "ash/shell_observer.h"
#include "ash/system/tray/tray_image_item.h"

namespace views {
class ImageView;
class View;
}

namespace ash {

class ASH_EXPORT AccessibilityObserver {
 public:
  virtual ~AccessibilityObserver() {}

  // Notifies when accessibilty mode changes.
  virtual void OnAccessibilityModeChanged() = 0;
};

namespace internal {

class TrayAccessibility : public TrayImageItem,
                          public AccessibilityObserver,
                          public ShellObserver {
 public:
  explicit TrayAccessibility(SystemTray* system_tray);
  virtual ~TrayAccessibility();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from AccessibilityObserver.
  virtual void OnAccessibilityModeChanged() OVERRIDE;

  // Overriden from ShellObserver.
  virtual void OnLoginStateChanged(user::LoginStatus status) OVERRIDE;

  views::View* default_;
  views::View* detailed_;

  bool request_popup_view_;
  bool accessibility_previously_enabled_;
  user::LoginStatus login_;

  DISALLOW_COPY_AND_ASSIGN(TrayAccessibility);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_ACCESSIBILITY_H_
