// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_menu_button.h"

#include "chrome/browser/ui/profile_menu_model.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/color_utils.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/menu_2.h"

// Menu should display below the profile button tag image on the frame. This
// offset size depends on whether the frame is in glass or opaque mode.
const int kMenuDisplayOffset = 7;

// TextHover is slightly darker than enabled color, for a subtle hover shift.
const SkColor kTextHover = 0xFFDDDDDD;
const SkColor kTextEnabled = SK_ColorWHITE;
const SkColor kTextHighlighted = SK_ColorWHITE;

// Horizontal padding beside profile menu button, to center it in the
// underlying tag image.
const int kProfileButtonBorderSpacing = 10;

// Maximum width for name string in pixels.
const int kMaxTextWidth = 200;

ProfileMenuButton::ProfileMenuButton(const std::wstring& text, Profile* profile)
    : MenuButton(NULL, text, this, true) {
  // Turn off hover highlighting and position button in the center of the
  // underlying profile tag image.
  set_border(views::Border::CreateEmptyBorder(
      0, kProfileButtonBorderSpacing, 0, kProfileButtonBorderSpacing));
  SetHoverColor(kTextHover);
  SetEnabledColor(kTextEnabled);
  SetHighlightColor(kTextHighlighted);

  profile_menu_model_.reset(new ProfileMenuModel);
  menu_.reset(new views::Menu2(profile_menu_model_.get()));
}

ProfileMenuButton::~ProfileMenuButton() {}

void ProfileMenuButton::SetText(const std::wstring& text) {
  MenuButton::SetText(UTF16ToWideHack(ui::ElideText(WideToUTF16Hack(text),
                      font(), kMaxTextWidth, false)));
}

// views::ViewMenuDelegate implementation
void ProfileMenuButton::RunMenu(views::View* source, const gfx::Point &pt) {
  gfx::Point menu_point(pt.x(), pt.y() + kMenuDisplayOffset);
  menu_->RunMenuAt(menu_point, views::Menu2::ALIGN_TOPRIGHT);
}
