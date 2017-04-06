// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/harmony_typography_provider.h"

#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font.h"

const gfx::FontList& HarmonyTypographyProvider::GetFont(int text_context,
                                                        int text_style) const {
  // "Target" font size constants from the Harmony spec.
  constexpr int kHeadlineSize = 20;
  constexpr int kTitleSize = 15;
  constexpr int kBodyTextLargeSize = 13;
  constexpr int kDefaultSize = 12;

#if defined(OS_WIN)
  constexpr gfx::Font::Weight kButtonFontWeight = gfx::Font::Weight::BOLD;
#else
  constexpr gfx::Font::Weight kButtonFontWeight = gfx::Font::Weight::MEDIUM;
#endif

  int size_delta = kDefaultSize - gfx::PlatformFont::kDefaultBaseFontSize;
  gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL;
  switch (text_context) {
    case CONTEXT_HEADLINE:
      size_delta = kHeadlineSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_DIALOG_TITLE:
      size_delta = kTitleSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_BODY_TEXT_LARGE:
      size_delta = kBodyTextLargeSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case views::style::CONTEXT_BUTTON:
      font_weight = kButtonFontWeight;
      break;
    default:
      break;
  }

  // Ignore |text_style| since it only affects color in the Harmony spec.
  return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      size_delta, gfx::Font::NORMAL, font_weight);
}

SkColor HarmonyTypographyProvider::GetColor(int text_context,
                                            int text_style) const {
  // TODO(tapted): Look up colors from the spec.
  return SK_ColorBLACK;
}

int HarmonyTypographyProvider::GetLineHeight(int text_context,
                                             int text_style) const {
  // "Target" line height constants from the Harmony spec. A default OS
  // configuration should use these heights. However, if the user overrides OS
  // defaults, then GetLineHeight() should return the height that would add the
  // same extra space between lines as the default configuration would have.
  constexpr int kHeadlineHeight = 32;
  constexpr int kTitleHeight = 22;
  constexpr int kBodyHeight = 20;  // For both large and small.

  // Button text should always use the minimum line height for a font to avoid
  // unnecessarily influencing the height of a button.
  constexpr int kButtonAbsoluteHeight = 0;

// The platform-specific heights (i.e. gfx::Font::GetHeight()) that result when
// asking for the target size constants in HarmonyTypographyProvider::GetFont()
// in a default OS configuration.
// TODO(tapted): Update these with constants specific to an OS point version.
#if defined(OS_MACOSX)
  constexpr int kHeadlinePlatformHeight = 25;
  constexpr int kTitlePlatformHeight = 19;
  constexpr int kBodyTextLargePlatformHeight = 16;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#elif defined(OS_WIN)
  constexpr int kHeadlinePlatformHeight = 28;
  constexpr int kTitlePlatformHeight = 20;
  constexpr int kBodyTextLargePlatformHeight = 17;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#else
  constexpr int kHeadlinePlatformHeight = 24;
  constexpr int kTitlePlatformHeight = 18;
  constexpr int kBodyTextLargePlatformHeight = 17;
  constexpr int kBodyTextSmallPlatformHeight = 15;
#endif

  // The style of the system font used to determine line heights.
  constexpr int kTemplateStyle = views::style::STYLE_PRIMARY;

  // TODO(tapted): These statics should be cleared out when something invokes
  // ResourceBundle::ReloadFonts(). Currently that only happens on ChromeOS.
  // See http://crbug.com/708943.
  static const int headline_height =
      GetFont(CONTEXT_HEADLINE, kTemplateStyle).GetHeight() -
      kHeadlinePlatformHeight + kHeadlineHeight;
  static const int title_height =
      GetFont(views::style::CONTEXT_DIALOG_TITLE, kTemplateStyle).GetHeight() -
      kTitlePlatformHeight + kTitleHeight;
  static const int body_large_height =
      GetFont(CONTEXT_BODY_TEXT_LARGE, kTemplateStyle).GetHeight() -
      kBodyTextLargePlatformHeight + kBodyHeight;
  static const int default_height =
      GetFont(CONTEXT_BODY_TEXT_SMALL, kTemplateStyle).GetHeight() -
      kBodyTextSmallPlatformHeight + kBodyHeight;

  switch (text_context) {
    case CONTEXT_HEADLINE:
      return headline_height;
    case views::style::CONTEXT_DIALOG_TITLE:
      return title_height;
    case CONTEXT_BODY_TEXT_LARGE:
      return body_large_height;
    case views::style::CONTEXT_BUTTON:
      return kButtonAbsoluteHeight;
    default:
      return default_height;
  }
}
