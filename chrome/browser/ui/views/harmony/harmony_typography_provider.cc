// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/harmony_typography_provider.h"

#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/platform_font.h"
#include "ui/native_theme/native_theme.h"

#if defined(USE_ASH)
#include "ash/public/cpp/ash_typography.h"  // nogncheck
#endif

const gfx::FontList& HarmonyTypographyProvider::GetFont(int context,
                                                        int style) const {
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

#if defined(USE_ASH)
  ash::ApplyAshFontStyles(context, style, &size_delta, &font_weight);
#endif

  switch (context) {
    case views::style::CONTEXT_BUTTON_MD:
      font_weight = WeightNotLighterThanNormal(kButtonFontWeight);
      break;
    case views::style::CONTEXT_DIALOG_TITLE:
      size_delta = kTitleSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_BODY_TEXT_LARGE:
      size_delta = kBodyTextLargeSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    case CONTEXT_HEADLINE:
      size_delta = kHeadlineSize - gfx::PlatformFont::kDefaultBaseFontSize;
      break;
    default:
      break;
  }

  // Ignore |style| since it only affects color in the Harmony spec.

  return ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      size_delta, gfx::Font::NORMAL, font_weight);
}

SkColor HarmonyTypographyProvider::GetColor(
    int context,
    int style,
    const ui::NativeTheme& theme) const {
  const SkColor foreground_color =
      theme.GetSystemColor(ui::NativeTheme::kColorId_LabelEnabledColor);

  // If the default foreground color from the native theme isn't black, the rest
  // of the Harmony spec isn't going to work. TODO(tapted): Something more
  // generic would be nice here, but that requires knowing the background color
  // for the text. At the time of writing, very few UI surfaces need native-
  // themed typography with a custom native theme. Typically just incognito
  // browser windows, when the native theme is NativeThemeDarkAura.
  if (foreground_color != SK_ColorBLACK) {
    switch (style) {
      case views::style::STYLE_DISABLED:
      case STYLE_SECONDARY:
      case STYLE_HINT:
        return theme.GetSystemColor(
            ui::NativeTheme::kColorId_LabelDisabledColor);
      case views::style::STYLE_LINK:
        return theme.GetSystemColor(ui::NativeTheme::kColorId_LinkEnabled);
      case STYLE_RED:
        return foreground_color == SK_ColorWHITE ? gfx::kGoogleRed300
                                                 : gfx::kGoogleRed700;
      case STYLE_GREEN:
        return foreground_color == SK_ColorWHITE ? gfx::kGoogleGreen300
                                                 : gfx::kGoogleGreen700;
    }
    return foreground_color;
  }

  switch (style) {
    case views::style::STYLE_DIALOG_BUTTON_DEFAULT:
      return SK_ColorWHITE;
    case views::style::STYLE_DISABLED:
      return SkColorSetRGB(0x9e, 0x9e, 0x9e);
    case views::style::STYLE_LINK:
      return gfx::kGoogleBlue700;
    case STYLE_SECONDARY:
    case STYLE_HINT:
      return SkColorSetRGB(0x75, 0x75, 0x75);
    case STYLE_RED:
      return gfx::kGoogleRed700;
    case STYLE_GREEN:
      return gfx::kGoogleGreen700;
  }
  return SkColorSetRGB(0x21, 0x21, 0x21);  // Primary for everything else.
}

int HarmonyTypographyProvider::GetLineHeight(int context, int style) const {
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

  switch (context) {
    case views::style::CONTEXT_BUTTON:
    case views::style::CONTEXT_BUTTON_MD:
      return kButtonAbsoluteHeight;
    case views::style::CONTEXT_DIALOG_TITLE:
      return title_height;
    case CONTEXT_BODY_TEXT_LARGE:
      return body_large_height;
    case CONTEXT_HEADLINE:
      return headline_height;
    default:
      return default_height;
  }
}
