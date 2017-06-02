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
  normal_scheme.horizon = 0xFF999999;
  normal_scheme.floor = 0xFF8C8C8C;
  normal_scheme.ceiling = normal_scheme.floor;
  normal_scheme.floor_grid = 0x26FFFFFF;
  normal_scheme.background = 0xCCB3B3B3;
  normal_scheme.background_hover = 0xFFCCCCCC;
  normal_scheme.background_down = 0xFFF3F3F3;
  normal_scheme.foreground = 0xFF333333;
  normal_scheme.emphasized = 0xFF000000;
  normal_scheme.deemphasized = 0xFF5A5A5A;
  normal_scheme.separator = 0xFF9E9E9E;
  normal_scheme.secure = gfx::kGoogleGreen700;
  normal_scheme.insecure = gfx::kGoogleRed700;
  normal_scheme.warning = normal_scheme.deemphasized;
  normal_scheme.disabled = 0x33333333;
  normal_scheme.loading_foreground = normal_scheme.foreground;
  normal_scheme.loading_background = normal_scheme.floor;

  kColorSchemes[ColorScheme::kModeFullscreen] =
      kColorSchemes[ColorScheme::kModeNormal];
  ColorScheme& fullscreen_scheme = kColorSchemes[ColorScheme::kModeFullscreen];
  fullscreen_scheme.horizon = 0xFF000714;
  fullscreen_scheme.floor = 0xFF070F1C;
  fullscreen_scheme.ceiling = 0xFF04080F;
  fullscreen_scheme.floor_grid = 0x40A3E0FF;

  ColorScheme& incognito_scheme = kColorSchemes[ColorScheme::kModeIncognito];
  incognito_scheme.horizon = 0xFF2E2E2E;
  incognito_scheme.floor = 0xFF282828;
  incognito_scheme.ceiling = 0xFF2F2F2F;
  incognito_scheme.floor_grid = 0xCC595959;
  incognito_scheme.foreground = 0xFFBCBCBC;
  incognito_scheme.emphasized = 0xFFEDEDED;
  incognito_scheme.deemphasized = 0xFF878787;
  incognito_scheme.background = 0xCC454545;
  incognito_scheme.background_hover = 0xCC505050;
  incognito_scheme.background_down = 0xCC888888;
  incognito_scheme.separator = 0xFF474747;
  incognito_scheme.secure = incognito_scheme.emphasized;
  incognito_scheme.insecure = incognito_scheme.emphasized;
  incognito_scheme.warning = incognito_scheme.emphasized;
  incognito_scheme.disabled = 0x33E6E6E6;
  normal_scheme.loading_foreground = 0xFF454545;
  normal_scheme.loading_background = 0xFF8A8A8A;

  initialized = true;
}

}  // namespace

const ColorScheme& ColorScheme::GetColorScheme(ColorScheme::Mode mode) {
  InitializeColorSchemes();
  return kColorSchemes[static_cast<int>(mode)];
}

}  // namespace vr_shell
