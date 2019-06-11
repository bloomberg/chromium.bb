// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_

#include "ash/public/interfaces/accessibility_controller_enums.mojom.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class AutoclickMenuButton;

// View for the Automatic Clicks menu bubble, which holds the Automatic Clicks
// Menu.
class AutoclickMenuBubbleView : public TrayBubbleView {
 public:
  AutoclickMenuBubbleView(TrayBubbleView::InitParams init_params);
  ~AutoclickMenuBubbleView() override;

  // TrayBubbleView:
  bool IsAnchoredToStatusArea() const override;

  // views::View:
  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuBubbleView);
};

// View for the Automatic Clicks Menu, which creates and manages
// individual buttons to control Automatic Clicks settings.
class AutoclickMenuView : public views::View, public views::ButtonListener {
 public:
  // Used for testing. Start at 1 because a view IDs should not be 0.
  enum class ButtonId {
    kPosition = 1,
    kLeftClick = 2,
    kRightClick = 3,
    kDoubleClick = 4,
    kDragAndDrop = 5,
    kPause = 6
  };

  AutoclickMenuView(mojom::AutoclickEventType type,
                    mojom::AutoclickMenuPosition position);
  ~AutoclickMenuView() override = default;

  void UpdateEventType(mojom::AutoclickEventType type);
  void UpdatePosition(mojom::AutoclickMenuPosition position);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // Unowned. Owned by views hierarchy.
  AutoclickMenuButton* left_click_button_;
  AutoclickMenuButton* right_click_button_;
  AutoclickMenuButton* double_click_button_;
  AutoclickMenuButton* drag_button_;
  AutoclickMenuButton* pause_button_;
  AutoclickMenuButton* position_button_;

  // The most recently selected event_type_ excluding kNoAction. This is used
  // when the pause button is selected in order to unpause and reset to the
  // previous state.
  mojom::AutoclickEventType event_type_ = mojom::AutoclickEventType::kLeftClick;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
