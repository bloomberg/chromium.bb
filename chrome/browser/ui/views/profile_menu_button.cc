// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profile_menu_button.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/color_utils.h"
#include "views/controls/button/button.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace {

// DefaultActiveTextShadow is a darkened blue color that works with Windows
// default theme background coloring.
const SkColor kDefaultActiveTextShadow = 0xFF708DB3;
// InactiveTextShadow is a light gray for inactive default themed buttons.
const SkColor kDefaultInactiveTextShadow = SK_ColorLTGRAY;
// DarkTextShadow is used to shadow names on themed browser frames.
const SkColor kDarkTextShadow = SK_ColorDKGRAY;
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

ProfileMenuButton::ProfileMenuButton(views::ButtonListener* listener,
                                     const std::wstring& text,
                                     views::ViewMenuDelegate* menu_delegate,
                                     Profile* profile)
    : MenuButton(listener, text, menu_delegate, true) {
  // Turn off hover highlighting and position button in the center of the
  // underlying profile tag image.
  set_border(views::Border::CreateEmptyBorder(
      0, kProfileButtonBorderSpacing, 0, kProfileButtonBorderSpacing));

  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile);
  if (theme_service->UsingDefaultTheme() ||
      theme_service->UsingNativeTheme()) {
    SetTextShadowColors(kDefaultActiveTextShadow, kDefaultInactiveTextShadow);
  } else {
    SetTextShadowColors(kDarkTextShadow, kDarkTextShadow);
  }
  SetHoverColor(kTextHover);
  SetEnabledColor(kTextEnabled);
  SetHighlightColor(kTextHighlighted);
}

ProfileMenuButton::~ProfileMenuButton() {}

void ProfileMenuButton::SetText(const std::wstring& text) {
  MenuButton::SetText(UTF16ToWideHack(ui::ElideText(WideToUTF16Hack(text),
                      font(), kMaxTextWidth, false)));
}

