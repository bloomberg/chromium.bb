// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/chrome_typography.h"

#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"

const gfx::FontList& LegacyTypographyProvider::GetFont(int context,
                                                       int style) const {
  constexpr int kHeadlineDelta = 8;
  constexpr int kDialogMessageDelta = 1;

  int size_delta;
  gfx::Font::Weight font_weight;
  GetDefaultFont(context, style, &size_delta, &font_weight);

#if defined(USE_ASH)
  ash::ApplyAshFontStyles(context, style, &size_delta, &font_weight);
#endif

  switch (context) {
    case CONTEXT_HEADLINE:
      size_delta = kHeadlineDelta;
      break;
    case CONTEXT_BODY_TEXT_LARGE:
      // Note: Not using ui::kMessageFontSizeDelta, so 13pt in most cases.
      size_delta = kDialogMessageDelta;
      break;
    case CONTEXT_BODY_TEXT_SMALL:
      size_delta = ui::kLabelFontSizeDelta;
      break;
    case CONTEXT_DEPRECATED_SMALL:
      size_delta = ui::ResourceBundle::kSmallFontDelta;
      break;
  }

  switch (style) {
    case STYLE_EMPHASIZED:
      font_weight = gfx::Font::Weight::BOLD;
      break;
  }
  constexpr gfx::Font::FontStyle kFontStyle = gfx::Font::NORMAL;
  return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      size_delta, kFontStyle, font_weight);
}

SkColor LegacyTypographyProvider::GetColor(int context,
                                           int style,
                                           const ui::NativeTheme& theme) const {
  // Use "disabled grey" for HINT and SECONDARY when Harmony is disabled.
  if (style == STYLE_HINT || style == STYLE_SECONDARY)
    style = views::style::STYLE_DISABLED;

  return DefaultTypographyProvider::GetColor(context, style, theme);
}
