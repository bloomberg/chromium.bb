// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/color_scheme.h"

#include "ui/gfx/color_palette.h"

namespace vr {

namespace {

ColorScheme gColorSchemes[ColorScheme::kNumModes];

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = gColorSchemes[ColorScheme::kModeNormal];
  normal_scheme.world_background = 0xFF999999;
  normal_scheme.world_background_text = 0xFF363636;
  normal_scheme.floor = 0xFF8C8C8C;
  normal_scheme.ceiling = normal_scheme.floor;
  normal_scheme.floor_grid = 0x26FFFFFF;
  normal_scheme.element_foreground = 0xFF333333;
  normal_scheme.element_background = 0xCCB3B3B3;
  normal_scheme.element_background_hover = 0xFFCCCCCC;
  normal_scheme.element_background_down = 0xFFF3F3F3;
  normal_scheme.button_colors.foreground = normal_scheme.element_foreground;
  normal_scheme.button_colors.background = normal_scheme.element_background;
  normal_scheme.button_colors.background_hover =
      normal_scheme.element_background_hover;
  normal_scheme.button_colors.background_press =
      normal_scheme.element_background_down;
  normal_scheme.loading_indicator_foreground = 0xFF2979FF;
  normal_scheme.loading_indicator_background = 0xFF454545;
  normal_scheme.exit_warning_foreground = SK_ColorWHITE;
  normal_scheme.exit_warning_background = 0xCC1A1A1A;
  normal_scheme.web_vr_transient_toast_foreground = 0xFFF3F3F3;
  normal_scheme.web_vr_transient_toast_background = SK_ColorBLACK;
  normal_scheme.exclusive_screen_toast_foreground = 0xCCFFFFFF;
  normal_scheme.exclusive_screen_toast_background = 0xCC2F2F2F;

  normal_scheme.system_indicator_foreground = 0xFF444444;
  normal_scheme.system_indicator_background = SK_ColorWHITE;
  normal_scheme.audio_permission_prompt_icon_foreground = 0xFF4285F4;
  normal_scheme.audio_permission_prompt_secondary_button_forground = 0xFF4285F4;
  normal_scheme.audio_permission_prompt_secondary_button_hover = 0x19999999;
  normal_scheme.audio_permission_prompt_secondary_button_down = 0x33999999;
  normal_scheme.audio_permission_prompt_primary_button_background = 0xFF4285F4;
  normal_scheme.audio_permission_prompt_primary_button_hover = 0xFF3E7DE6;
  normal_scheme.audio_permission_prompt_primary_button_down = 0xFF3E7DE6;
  normal_scheme.audio_permission_prompt_background = 0xFFF5F5F5;
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
  normal_scheme.url_emphasized = SK_ColorBLACK;
  normal_scheme.url_deemphasized = 0xFF5A5A5A;
  normal_scheme.offline_page_warning = 0xFF242424;
  normal_scheme.disabled = 0x33333333;
  normal_scheme.dimmer_inner = 0xCC0D0D0D;
  normal_scheme.dimmer_outer = 0xE6000000;
  normal_scheme.splash_screen_background = SK_ColorBLACK;
  normal_scheme.splash_screen_text_color = 0xA6FFFFFF;
  normal_scheme.spinner_background = SK_ColorBLACK;
  normal_scheme.spinner_color = 0xFFF3F3F3;
  normal_scheme.timeout_message_background = 0xFF444444;
  normal_scheme.timeout_message_foreground = normal_scheme.spinner_color;
  normal_scheme.speech_recognition_circle_background = 0xFF4285F4;

  gColorSchemes[ColorScheme::kModeFullscreen] =
      gColorSchemes[ColorScheme::kModeNormal];
  ColorScheme& fullscreen_scheme = gColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.world_background = 0xFF000714;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x40A3E0FF;
  fullscreen_scheme.element_foreground = 0x80FFFFFF;
  fullscreen_scheme.element_background = 0xCC2B3E48;
  fullscreen_scheme.element_background_hover = 0xCC536B77;
  fullscreen_scheme.element_background_down = 0xCC96AFBB;

  fullscreen_scheme.button_colors.foreground =
      fullscreen_scheme.element_foreground;
  fullscreen_scheme.button_colors.background =
      fullscreen_scheme.element_background;
  fullscreen_scheme.button_colors.background_hover =
      fullscreen_scheme.element_background_hover;
  fullscreen_scheme.button_colors.background_press =
      fullscreen_scheme.element_background_down;

  gColorSchemes[ColorScheme::kModeIncognito] =
      gColorSchemes[ColorScheme::kModeNormal];
  ColorScheme& incognito_scheme = gColorSchemes[ColorScheme::kModeIncognito];
  incognito_scheme.world_background = 0xFF2E2E2E;
  incognito_scheme.world_background_text = 0xFF878787;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xCC595959;
  incognito_scheme.element_foreground = 0xFFBCBCBC;
  incognito_scheme.element_background = 0xCC454545;
  incognito_scheme.element_background_hover = 0xCC505050;
  incognito_scheme.element_background_down = 0xCC888888;
  incognito_scheme.button_colors.foreground =
      fullscreen_scheme.element_foreground;
  incognito_scheme.button_colors.background =
      fullscreen_scheme.element_background;
  incognito_scheme.button_colors.background_hover =
      fullscreen_scheme.element_background_hover;
  incognito_scheme.button_colors.background_press =
      fullscreen_scheme.element_background_down;
  incognito_scheme.separator = 0xFF474747;
  incognito_scheme.secure = 0xFFEDEDED;
  incognito_scheme.insecure = incognito_scheme.secure;
  incognito_scheme.url_emphasized = incognito_scheme.secure;
  incognito_scheme.url_deemphasized = 0xFF878787;
  incognito_scheme.offline_page_warning = incognito_scheme.secure;
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

bool ButtonColors::operator==(const ButtonColors& other) const {
  return background == other.background &&
         background_hover == other.background_hover &&
         background_press == other.background_press &&
         foreground == other.foreground;
}

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return gColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr
