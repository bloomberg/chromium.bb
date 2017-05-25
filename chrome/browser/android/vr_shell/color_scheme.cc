// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/color_scheme.h"

namespace vr_shell {

namespace {

static ColorScheme kColorSchemes[ColorScheme::kNumModes];

void InitializeColorSchemes() {
  static bool initialized = false;
  if (initialized)
    return;

  ColorScheme& normal_scheme = kColorSchemes[ColorScheme::kModeNormal];
  normal_scheme.horizon = {0.89, 0.89, 0.89, 1.0};
  normal_scheme.floor = {0.811, 0.811, 0.811, 1.0};
  normal_scheme.ceiling = {0.859, 0.859, 0.859, 1.0};
  normal_scheme.floor_grid = {1.0, 1.0, 1.0, 1.0};

  ColorScheme& fullscreen_scheme = kColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.horizon = {0.039215686, 0.0, 0.082352941, 1.0};
  fullscreen_scheme.floor = {0.02745098, 0.058823529, 0.109803922, 1.0};
  fullscreen_scheme.ceiling = {0.015686275, 0.031372549, 0.058823529, 1.0};
  fullscreen_scheme.floor_grid = {0.639215686, 0.878431373, 1.0, 0.5};

  initialized = true;
}

}  // namespace

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return kColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr_shell
