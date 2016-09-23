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

// TODO(bruthig): Consider adding an 'inactive' color to the NativeTheme
// and allow Label to use it directly. This would require changing the
// View::enabled_ flag to a tri-state enum.
const SkColor kInactiveTextColor = SkColorSetRGB(0x64, 0x64, 0x64);
}  // namespace

TrayPopupItemStyle::TrayPopupItemStyle(const ui::NativeTheme* theme,
                                       FontStyle font_style)
    : theme_(theme),
      font_style_(font_style),
      color_style_(ColorStyle::ACTIVE) {}

TrayPopupItemStyle::~TrayPopupItemStyle() {}

SkColor TrayPopupItemStyle::GetForegroundColor() const {
  switch (color_style_) {
    case ColorStyle::ACTIVE:
      return theme_->GetSystemColor(
          ui::NativeTheme::kColorId_LabelEnabledColor);
    case ColorStyle::INACTIVE:
      return kInactiveTextColor;
    case ColorStyle::DISABLED:
      return theme_->GetSystemColor(
          ui::NativeTheme::kColorId_LabelDisabledColor);
  }
  NOTREACHED();
  // Use a noticeable color to help notice unhandled cases.
  return SK_ColorMAGENTA;
}

void TrayPopupItemStyle::SetupLabel(views::Label* label) const {
  label->SetEnabledColor(GetForegroundColor());

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
