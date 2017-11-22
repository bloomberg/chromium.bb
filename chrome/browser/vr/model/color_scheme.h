// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
#define CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_

#include "third_party/skia/include/core/SkColor.h"

namespace vr {

struct ButtonColors {
  bool operator==(const ButtonColors& other) const;
  bool operator!=(const ButtonColors& other) const;

  SkColor GetBackgroundColor(bool hovered, bool pressed) const;
  SkColor GetForegroundColor(bool disabled) const;

  SkColor background = SK_ColorBLACK;
  SkColor background_hover = SK_ColorBLACK;
  SkColor background_down = SK_ColorBLACK;
  SkColor foreground = SK_ColorBLACK;
  SkColor foreground_disabled = SK_ColorBLACK;
};

struct UrlBarColors {
  bool operator==(const UrlBarColors& other) const;
  bool operator!=(const UrlBarColors& other) const;
  SkColor deemphasized = SK_ColorBLACK;
  SkColor emphasized = SK_ColorBLACK;
  SkColor secure = SK_ColorBLACK;
  SkColor insecure = SK_ColorBLACK;
  SkColor offline_page_warning = SK_ColorBLACK;
  SkColor separator = SK_ColorBLACK;
  ButtonColors back_button;
};

struct ColorScheme {
  enum Mode : int {
    kModeNormal = 0,
    kModeFullscreen,
    kModeIncognito,
    kNumModes,
  };

  static const ColorScheme& GetColorScheme(Mode mode);

  ColorScheme();
  ColorScheme(const ColorScheme& other);

  // These colors should be named generically, if possible, so that they can be
  // meaningfully reused by multiple elements.
  SkColor world_background;
  SkColor world_background_text;
  SkColor floor;
  SkColor ceiling;
  SkColor floor_grid;

  // The foreground color is used for text and sometimes for icons.
  SkColor element_foreground;
  // The background color is used behind text or icons in the foreground color.
  // The related hover and down colors are to be used for buttons.
  SkColor element_background;
  SkColor element_background_hover;
  SkColor element_background_down;

  // Specific element background and foregrounds
  ButtonColors button_colors;
  SkColor loading_indicator_foreground;
  SkColor loading_indicator_background;
  SkColor exit_warning_foreground;
  SkColor exit_warning_background;
  SkColor web_vr_transient_toast_foreground;
  SkColor web_vr_transient_toast_background;
  SkColor exclusive_screen_toast_foreground;
  SkColor exclusive_screen_toast_background;
  SkColor system_indicator_foreground;
  SkColor system_indicator_background;
  SkColor audio_permission_prompt_icon_foreground;
  SkColor audio_permission_prompt_background;
  ButtonColors audio_permission_prompt_secondary_button_colors;
  ButtonColors audio_permission_prompt_primary_button_colors;

  // The colors used for text and buttons on prompts.
  SkColor prompt_foreground;
  ButtonColors prompt_secondary_button_colors;
  ButtonColors prompt_primary_button_colors;

  UrlBarColors url_bar;

  // Screen dimmer colors.
  SkColor dimmer_outer;
  SkColor dimmer_inner;

  // Splash screen colors.
  SkColor splash_screen_background;
  SkColor splash_screen_text_color;

  // WebVr timeout spinner colors.
  SkColor spinner_background;
  SkColor spinner_color;

  // Timeout UI colors
  SkColor timeout_message_background;
  SkColor timeout_message_foreground;

  SkColor speech_recognition_circle_background;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_MODEL_COLOR_SCHEME_H_
