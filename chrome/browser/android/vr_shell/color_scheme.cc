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
  normal_scheme.horizon = 0xFFE3E3E3;
  normal_scheme.floor = 0xFFCFCFCF;
  normal_scheme.ceiling = 0xFFDBDBDB;
  normal_scheme.floor_grid = SK_ColorWHITE;
  normal_scheme.background = 0x66EBEBEB;
  normal_scheme.background_hover = 0xFFEAEAEA;
  normal_scheme.background_down = 0xFFFAFAFA;
  normal_scheme.foreground = 0xFF333333;
  normal_scheme.emphasized = 0xFF000000;
  normal_scheme.deemphasized = 0xFF5A5A5A;
  normal_scheme.separator = 0x33000000;
  normal_scheme.secure = gfx::kGoogleGreen700;
  normal_scheme.insecure = gfx::kGoogleRed700;
  normal_scheme.warning = normal_scheme.deemphasized;
  normal_scheme.disabled = 0x33333333;

  kColorSchemes[ColorScheme::kModeFullscreen] =
      kColorSchemes[ColorScheme::kModeNormal];
  ColorScheme& fullscreen_scheme = kColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.horizon = 0xFF0A0015;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x80A3E0FF;

  // TODO(joshcarpenter): Update these per spec.
  ColorScheme& incognito_scheme = kColorSchemes[ColorScheme::kModeIncognito];
  incognito_scheme.horizon = 0xFF2E2E2E;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xFF595959;
  incognito_scheme.foreground = 0xFFE6E6E6;
  incognito_scheme.emphasized = 0xFFFFFFFF;
  incognito_scheme.deemphasized = incognito_scheme.foreground;
  incognito_scheme.background = 0xB9454545;
  incognito_scheme.background_hover = 0xD9454545;
  incognito_scheme.background_down = 0xFF454545;
  incognito_scheme.separator = 0xDD595959;
  incognito_scheme.secure = incognito_scheme.foreground;
  incognito_scheme.insecure = incognito_scheme.foreground;
  incognito_scheme.warning = incognito_scheme.foreground;
  incognito_scheme.disabled = 0x33E6E6E6;

  initialized = true;
}

}  // namespace

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return kColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr_shell
