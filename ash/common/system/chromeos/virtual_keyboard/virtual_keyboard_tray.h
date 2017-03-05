// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_

#include "ash/common/keyboard/keyboard_ui_observer.h"
#include "ash/common/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "ui/keyboard/keyboard_controller_observer.h"

namespace views {
class ImageView;
}

namespace ash {

// TODO(sky): make this visible on non-chromeos platforms.
class VirtualKeyboardTray : public TrayBackgroundView,
                            public KeyboardUIObserver,
                            public keyboard::KeyboardControllerObserver {
 public:
  explicit VirtualKeyboardTray(WmShelf* wm_shelf);
  ~VirtualKeyboardTray() override;

  // TrayBackgroundView:
  void SetShelfAlignment(ShelfAlignment alignment) override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const views::TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

  // KeyboardUIObserver:
  void OnKeyboardEnabledStateChanged(bool new_enabled) override;

  // keyboard::KeyboardControllerObserver:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override;
  void OnKeyboardClosed() override;

 private:
  // Creates a new border for the icon. The padding is determined based on the
  // alignment of the shelf.
  void SetIconBorderForShelfAlignment();

  void ObserveKeyboardController();
  void UnobserveKeyboardController();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  WmShelf* wm_shelf_;

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardTray);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
