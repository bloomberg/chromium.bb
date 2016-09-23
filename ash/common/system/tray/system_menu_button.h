// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
#define ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace views {
class InkDropHighlight;
class InkDropRipple;
}

namespace ash {

// A 48x48 image button with a material design ripple effect, which can be
// used across Ash material design native UI menus.
// TODO(tdanderson): Deprecate TrayPopupHeaderButton in favor of
// SystemMenuButton once material design is enabled by default. See
// crbug.com/614453.
class SystemMenuButton : public views::ImageButton {
 public:
  // Constructs the button with |listener| and a centered icon corresponding to
  // |icon|. |accessible_name_id| corresponds to the string in
  // ui::ResourceBundle to use for the button's accessible and tooltip text.
  SystemMenuButton(views::ButtonListener* listener,
                   const gfx::VectorIcon& icon,
                   int accessible_name_id);
  ~SystemMenuButton() override;

  // views::ImageButton:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  bool ShouldShowInkDropForFocus() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMenuButton);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
