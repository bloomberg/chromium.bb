// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_menu_button.h"

#include "ui/base/text/text_elider.h"
#include "ui/gfx/color_utils.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace {

// ActiveTextShadow is a darkened version of hsl_active_shift.
const SkColor kActiveTextShadow = 0xFF708DB3;
// InactiveTextShadow is slightly darker than grey-white inactive background.
const SkColor kInactiveTextShadow = SK_ColorLTGRAY;
// TextHover is slightly darker than enabled color, for a subtle hover shift.
const SkColor kTextHover = 0xFFDDDDDD;
const SkColor kTextEnabled = SK_ColorWHITE;
const SkColor kTextHighlighted = SK_ColorWHITE;

// Horizontal padding beside profile menu button, to center it in the
// underlying tag image.
const int kProfileButtonBorderSpacing = 10;

// Maximum width for name string in pixels.
const int kMaxTextWidth = 200;
}

namespace views {

ProfileMenuButton::ProfileMenuButton(ButtonListener* listener,
                                     const std::wstring& text,
                                     ViewMenuDelegate* menu_delegate) :
    MenuButton(listener, text, menu_delegate, true) {
  // Turn off hover highlighting and position button in the center of the
  // underlying profile tag image.
  set_border(views::Border::CreateEmptyBorder(
      0, kProfileButtonBorderSpacing, 0, kProfileButtonBorderSpacing));
  SetTextShadowColors(kActiveTextShadow, kInactiveTextShadow);
  SetHoverColor(kTextHover);
  SetEnabledColor(kTextEnabled);
  SetHighlightColor(kTextHighlighted);
}

ProfileMenuButton::~ProfileMenuButton() {}

void ProfileMenuButton::SetText(const std::wstring& text) {
  MenuButton::SetText(ui::ElideText(text, font(), kMaxTextWidth, false));
}

}  // namespace views

