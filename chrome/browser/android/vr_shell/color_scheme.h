// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_

#include "third_party/skia/include/core/SkColor.h"

namespace vr_shell {

struct ColorScheme {
  // TODO(vollick): Add support for incognito.
  enum Mode : int {
    kModeNormal = 0,
    kModeFullscreen,
    kNumModes,
  };

  static const ColorScheme& GetColorScheme(Mode mode);

  // These colors should be named generically, if possible, so that they can be
  // meaningfully reused by multiple elements.
  SkColor horizon;
  SkColor floor;
  SkColor ceiling;
  SkColor floor_grid;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_COLOR_SCHEME_H_
