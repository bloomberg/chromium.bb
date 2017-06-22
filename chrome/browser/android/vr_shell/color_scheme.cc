// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/color_scheme.h"

#include "ui/gfx/color_palette.h"

namespace vr_shell {

namespace {

static ColorScheme kColorSchemes[ColorScheme::kNumModes];

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = kColorSchemes[ColorScheme::kModeNormal];
  normal_scheme.world_background = 0xFF999999;
  normal_scheme.floor = 0xFF8C8C8C;
  normal_scheme.ceiling = normal_scheme.floor;
  normal_scheme.floor_grid = 0x26FFFFFF;
  normal_scheme.element_foreground = 0xFF333333;
  normal_scheme.element_background = 0xCCB3B3B3;
  normal_scheme.element_background_hover = 0xFFCCCCCC;
  normal_scheme.element_background_down = 0xFFF3F3F3;
  normal_scheme.close_button_foreground = normal_scheme.element_foreground;
  normal_scheme.close_button_background = normal_scheme.element_background;
  normal_scheme.close_button_background_hover =
      normal_scheme.element_background_hover;
  normal_scheme.close_button_background_down =
      normal_scheme.element_background_down;
  normal_scheme.loading_indicator_foreground = normal_scheme.element_foreground;
  normal_scheme.loading_indicator_background = normal_scheme.floor;
  normal_scheme.exit_warning_foreground = SK_ColorWHITE;
  normal_scheme.exit_warning_background = 0xCC1A1A1A;
  normal_scheme.transient_warning_foreground =
      normal_scheme.exit_warning_foreground;
  normal_scheme.transient_warning_background =
      normal_scheme.exit_warning_background;
  normal_scheme.presentation_toast_foreground =
      normal_scheme.exit_warning_foreground;
  normal_scheme.presentation_toast_background =
      normal_scheme.exit_warning_background;

  normal_scheme.permanent_warning_foreground = 0xFF444444;
  normal_scheme.permanent_warning_background = SK_ColorWHITE;
  normal_scheme.system_indicator_foreground =
      normal_scheme.permanent_warning_foreground;
  normal_scheme.system_indicator_background =
      normal_scheme.permanent_warning_background;
  normal_scheme.separator = 0xFF9E9E9E;
  normal_scheme.prompt_foreground = 0xCC000000;
  normal_scheme.prompt_primary_button_forground = 0xA6000000;
  normal_scheme.prompt_secondary_button_foreground = 0xA6000000;
  normal_scheme.prompt_primary_button_background = 0xBFFFFFFF;
  normal_scheme.prompt_secondary_button_background = 0x66FFFFFF;
  normal_scheme.prompt_button_background_hover = 0xFFFFFFFF;
  normal_scheme.prompt_button_background_down = 0xE6FFFFFF;
  normal_scheme.secure = gfx::kGoogleGreen700;
  normal_scheme.insecure = gfx::kGoogleRed700;
  normal_scheme.warning = 0xFF5A5A5A;
  normal_scheme.url_emphasized = SK_ColorBLACK;
  normal_scheme.url_deemphasized = normal_scheme.warning;
  normal_scheme.disabled = 0x33333333;
  normal_scheme.dimmer_inner = 0xCC0D0D0D;
  normal_scheme.dimmer_outer = 0xE6000000;

  kColorSchemes[ColorScheme::kModeFullscreen] =
      kColorSchemes[ColorScheme::kModeNormal];
  ColorScheme& fullscreen_scheme = kColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.world_background = 0xFF000714;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x40A3E0FF;
  fullscreen_scheme.element_foreground = 0x80FFFFFF;
  fullscreen_scheme.element_background = 0xCC2B3E48;
  fullscreen_scheme.element_background_hover = 0xCC536B77;
  fullscreen_scheme.element_background_down = 0xCC96AFBB;
  fullscreen_scheme.close_button_foreground =
      fullscreen_scheme.element_foreground;
  fullscreen_scheme.close_button_background =
      fullscreen_scheme.element_background;
  fullscreen_scheme.close_button_background_hover =
      fullscreen_scheme.element_background_hover;
  fullscreen_scheme.close_button_background_down =
      fullscreen_scheme.element_background_down;

  ColorScheme& incognito_scheme = kColorSchemes[ColorScheme::kModeIncognito];
  incognito_scheme.world_background = 0xFF2E2E2E;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xCC595959;
  incognito_scheme.element_foreground = 0xFFBCBCBC;
  incognito_scheme.element_background = 0xCC454545;
  incognito_scheme.element_background_hover = 0xCC505050;
  incognito_scheme.element_background_down = 0xCC888888;
  incognito_scheme.close_button_foreground =
      fullscreen_scheme.element_foreground;
  incognito_scheme.close_button_background =
      fullscreen_scheme.element_background;
  incognito_scheme.close_button_background_hover =
      fullscreen_scheme.element_background_hover;
  incognito_scheme.close_button_background_down =
      fullscreen_scheme.element_background_down;
  incognito_scheme.loading_indicator_foreground = 0xFF8A8A8A;
  incognito_scheme.loading_indicator_background = 0xFF454545;
  incognito_scheme.separator = 0xFF474747;
  incognito_scheme.secure = 0xFFEDEDED;
  incognito_scheme.insecure = incognito_scheme.secure;
  incognito_scheme.warning = incognito_scheme.secure;
  incognito_scheme.url_emphasized = incognito_scheme.secure;
  incognito_scheme.url_deemphasized = 0xFF878787;
  incognito_scheme.prompt_foreground = 0xCCFFFFFF;
  incognito_scheme.prompt_primary_button_forground = 0xD9000000;
  incognito_scheme.prompt_secondary_button_foreground = 0xD9000000;
  incognito_scheme.prompt_primary_button_background = 0xD9FFFFFF;
  incognito_scheme.prompt_secondary_button_background = 0x80FFFFFF;
  incognito_scheme.prompt_button_background_hover = 0xFF8C8C8C;
  incognito_scheme.prompt_button_background_down = 0xE6FFFFFF;
  incognito_scheme.disabled = 0x33E6E6E6;

  initialized = true;
}

}  // namespace

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return kColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr_shell
