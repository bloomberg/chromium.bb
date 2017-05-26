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
  normal_scheme.horizon = 0xFFE3E3E3;
  normal_scheme.floor = 0xFFCFCFCF;
  normal_scheme.ceiling = 0xFFDBDBDB;
  normal_scheme.floor_grid = SK_ColorWHITE;

  ColorScheme& fullscreen_scheme = kColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.horizon = 0xFF0A0015;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x80A3E0FF;

  initialized = true;
}

}  // namespace

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return kColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr_shell
