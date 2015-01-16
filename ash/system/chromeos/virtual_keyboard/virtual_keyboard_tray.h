// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
#define ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_

#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/user/login_status.h"
#include "base/basictypes.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageButton;
}

namespace ash {
class StatusAreaWidget;

class VirtualKeyboardTray : public TrayBackgroundView,
                            public views::ButtonListener,
                            public AccessibilityObserver {
 public:
  explicit VirtualKeyboardTray(StatusAreaWidget* status_area_widget);
  ~VirtualKeyboardTray() override;

  // TrayBackgroundView:
  void SetShelfAlignment(ShelfAlignment alignment) override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  bool ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // AccessibilityObserver:
  void OnAccessibilityModeChanged(
      ui::AccessibilityNotificationVisibility notify) override;

 private:
  views::ImageButton* button_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
