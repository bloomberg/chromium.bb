// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_item_style.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

// TODO(bruthig|tdanderson): Figure out why the theme providers aren't providing
// the desired colors. See http://crbug.com/614453.

// The base color used to compute the effective text colors.
const SkColor kBaseTextColor = SK_ColorBLACK;

const int kActiveTextAlpha = 0xDE;
const int kInactiveTextAlpha = 0x8A;
const int kDisabledTextAlpha = 0x61;

// The base color used to compute the effective icon colors.
const SkColor kBaseIconColor = SkColorSetRGB(0x5a, 0x5a, 0x5a);

const int kActiveIconAlpha = 0xFF;
const int kInactiveIconAlpha = 0x8A;
const int kDisabledIconAlpha = 0x61;

}  // namespace

TrayPopupItemStyle::TrayPopupItemStyle(const ui::NativeTheme* theme,
                                       FontStyle font_style)
    : theme_(theme),
      font_style_(font_style),
      color_style_(ColorStyle::ACTIVE) {}

TrayPopupItemStyle::~TrayPopupItemStyle() {}

SkColor TrayPopupItemStyle::GetTextColor() const {
  switch (color_style_) {
    case ColorStyle::ACTIVE:
      return SkColorSetA(kBaseTextColor, kActiveTextAlpha);
    case ColorStyle::INACTIVE:
      return SkColorSetA(kBaseTextColor, kInactiveTextAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(kBaseTextColor, kDisabledTextAlpha);
  }
  NOTREACHED();
  // Use a noticeable color to help notice unhandled cases.
  return SK_ColorMAGENTA;
}

SkColor TrayPopupItemStyle::GetIconColor() const {
  switch (color_style_) {
    case ColorStyle::ACTIVE:
      return SkColorSetA(kBaseIconColor, kActiveIconAlpha);
    case ColorStyle::INACTIVE:
      return SkColorSetA(kBaseIconColor, kInactiveIconAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(kBaseIconColor, kDisabledIconAlpha);
  }
  NOTREACHED();
  // Use a noticeable color to help notice unhandled cases.
  return SK_ColorMAGENTA;
}

void TrayPopupItemStyle::SetupLabel(views::Label* label) const {
  label->SetEnabledColor(GetTextColor());

  const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
  switch (font_style_) {
    case FontStyle::TITLE:
      label->SetFontList(base_font_list.Derive(2, gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      break;
    case FontStyle::DEFAULT_VIEW_LABEL:
      label->SetFontList(base_font_list.Derive(2, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::DETAILED_VIEW_LABEL:
    case FontStyle::SYSTEM_INFO:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::CAPTION:
      label->SetFontList(base_font_list.Derive(0, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::BUTTON:
      label->SetFontList(base_font_list.Derive(0, gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      break;
  }
}

}  // namespace ash
