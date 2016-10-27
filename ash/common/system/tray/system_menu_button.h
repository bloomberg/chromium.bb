// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
#define ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_

#include "ash/resources/vector_icons/vector_icons.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A 48x48 image button with a material design ripple effect, which can be
// used across Ash material design native UI menus.
// TODO(tdanderson): Deprecate TrayPopupHeaderButton in favor of
// SystemMenuButton once material design is enabled by default. See
// crbug.com/614453.
class SystemMenuButton : public views::ImageButton {
 public:
  // The different ink drop styles that can be used.
  enum class InkDropStyle {
    // Despite the poor name this style actually displays as a circle. It is
    // typically used for buttons that the user doesn't need to know the exact
    // targetable rect and they are expected to target an icon centered in the
    // targetable space.
    SQUARE,
    // Typically used for buttons that should indicate to the user what the
    // actual targetable bounds are. e.g. a row of buttons separated with
    // separators.
    FLOOD_FILL
  };

  // Constructs the button with |listener| and a centered icon corresponding to
  // |icon|. |ink_drop_style| specifies which flavor of the ink drop should be
  // used. |accessible_name_id| corresponds to the string in
  // ui::ResourceBundle to use for the button's accessible and tooltip text.
  SystemMenuButton(views::ButtonListener* listener,
                   InkDropStyle ink_drop_style,
                   const gfx::VectorIcon& icon,
                   int accessible_name_id);
  ~SystemMenuButton() override;

  // views::ImageButton:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  bool ShouldShowInkDropHighlight() const override;

 private:
  // Returns the size that the ink drop should be constructed with.
  gfx::Size GetInkDropSize() const;

  // Defines the flavor of ink drop ripple/highlight that should be constructed.
  InkDropStyle ink_drop_style_;

  DISALLOW_COPY_AND_ASSIGN(SystemMenuButton);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_SYSTEM_MENU_BUTTON_H_
