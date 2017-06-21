// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_

#include "third_party/skia/include/core/SkColor.h"

namespace vr_shell {

struct ColorScheme {
  enum Mode : int {
    kModeNormal = 0,
    kModeFullscreen,
    kModeIncognito,
    kNumModes,
  };

  static const ColorScheme& GetColorScheme(Mode mode);

  // These colors should be named generically, if possible, so that they can be
  // meaningfully reused by multiple elements.
  SkColor world_background;
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
  SkColor close_button_foreground;
  SkColor close_button_background;
  SkColor close_button_background_hover;
  SkColor close_button_background_down;
  SkColor loading_indicator_foreground;
  SkColor loading_indicator_background;
  SkColor exit_warning_foreground;
  SkColor exit_warning_background;
  SkColor transient_warning_foreground;
  SkColor transient_warning_background;
  SkColor permanent_warning_foreground;
  SkColor permanent_warning_background;
  SkColor system_indicator_foreground;
  SkColor system_indicator_background;

  // The colors used for text and buttons on prompts.
  SkColor prompt_foreground;
  SkColor prompt_primary_button_forground;
  SkColor prompt_secondary_button_foreground;
  SkColor prompt_primary_button_background;
  SkColor prompt_secondary_button_background;
  SkColor prompt_button_background_hover;
  SkColor prompt_button_background_down;

  // If you have a segmented element, its separators should use this color.
  SkColor separator;

  // Some content changes color based on the security level. Those visuals
  // should respect these colors.
  SkColor secure;
  SkColor insecure;
  SkColor warning;
  SkColor url_emphasized;
  SkColor url_deemphasized;

  // The color used for disabled icons.
  SkColor disabled;

  // Screen dimmer colors.
  SkColor dimmer_outer;
  SkColor dimmer_inner;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_
