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
  SkColor horizon;
  SkColor floor;
  SkColor ceiling;
  SkColor floor_grid;

  // The foreground color is used for text and sometimes for icons.
  SkColor foreground;
  SkColor emphasized;
  SkColor deemphasized;

  // This is the background color. To be used behind text in the foreground
  // color. The related hover and down colors are to be used for buttons.
  SkColor background;
  SkColor background_hover;
  SkColor background_down;

  // If you have a segmented element, its separators should use this color.
  SkColor separator;

  // Some content changes color based on the security level. Those visuals
  // should respect these colors.
  SkColor secure;
  SkColor insecure;
  SkColor warning;

  // The color used for disabled icons.
  SkColor disabled;

  // Colors used for the loading progress bar indicator.
  SkColor loading_background;
  SkColor loading_foreground;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_
