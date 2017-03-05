// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_item_style.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/label.h"

namespace ash {
namespace {

const int kInactiveAlpha = 0x8A;
const int kDisabledAlpha = 0x61;

}  // namespace

// static
SkColor TrayPopupItemStyle::GetIconColor(ColorStyle color_style) {
  switch (color_style) {
    case ColorStyle::ACTIVE:
      return gfx::kChromeIconGrey;
    case ColorStyle::INACTIVE:
      return SkColorSetA(gfx::kChromeIconGrey, kInactiveAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(gfx::kChromeIconGrey, kDisabledAlpha);
    case ColorStyle::CONNECTED:
      return gfx::kPlaceholderColor;
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

TrayPopupItemStyle::TrayPopupItemStyle(FontStyle font_style)
    : font_style_(font_style), color_style_(ColorStyle::ACTIVE) {
  if (font_style_ == FontStyle::SYSTEM_INFO)
    color_style_ = ColorStyle::INACTIVE;
}

TrayPopupItemStyle::~TrayPopupItemStyle() {}

SkColor TrayPopupItemStyle::GetTextColor() const {
  const SkColor kBaseTextColor = SkColorSetA(SK_ColorBLACK, 0xDE);

  switch (color_style_) {
    case ColorStyle::ACTIVE:
      return kBaseTextColor;
    case ColorStyle::INACTIVE:
      return SkColorSetA(kBaseTextColor, kInactiveAlpha);
    case ColorStyle::DISABLED:
      return SkColorSetA(kBaseTextColor, kDisabledAlpha);
    case ColorStyle::CONNECTED:
      return gfx::kGoogleGreen700;
  }
  NOTREACHED();
  return gfx::kPlaceholderColor;
}

SkColor TrayPopupItemStyle::GetIconColor() const {
  return GetIconColor(color_style_);
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
    case FontStyle::SUB_HEADER:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      label->SetEnabledColor(label->GetNativeTheme()->GetSystemColor(
          ui::NativeTheme::kColorId_ProminentButtonColor));
      label->SetAutoColorReadabilityEnabled(false);
      break;
    case FontStyle::DETAILED_VIEW_LABEL:
    case FontStyle::SYSTEM_INFO:
      label->SetFontList(base_font_list.Derive(1, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
    case FontStyle::BUTTON:
      label->SetFontList(base_font_list.Derive(0, gfx::Font::NORMAL,
                                               gfx::Font::Weight::MEDIUM));
      break;
    case FontStyle::CAPTION:
      label->SetFontList(base_font_list.Derive(0, gfx::Font::NORMAL,
                                               gfx::Font::Weight::NORMAL));
      break;
  }
}

}  // namespace ash
