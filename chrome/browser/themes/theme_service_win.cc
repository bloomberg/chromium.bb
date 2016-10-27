// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_win.h"

#include "base/bind.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/win/titlebar_config.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/shell.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

ThemeServiceWin::ThemeServiceWin() {
  // This just checks for Windows 10 instead of calling ShouldUseDwmFrameColor()
  // because we want to monitor the frame color even when a custom frame is in
  // use, so that it will be correct if at any time the user switches to the
  // native frame.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    dwm_key_.reset(new base::win::RegKey(
        HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", KEY_READ));
    if (dwm_key_->Valid())
      OnDwmKeyUpdated();
    else
      dwm_key_.reset();
  }
}

ThemeServiceWin::~ThemeServiceWin() {
}

bool ThemeServiceWin::ShouldUseNativeFrame() const {
  const bool use_native_frame_if_enabled =
      ShouldCustomDrawSystemTitlebar() || !HasCustomImage(IDR_THEME_FRAME);
  return use_native_frame_if_enabled && ui::win::IsAeroGlassEnabled();
}

SkColor ThemeServiceWin::GetDefaultColor(int id, bool incognito) const {
  if (ShouldUseDwmFrameColor()) {
    // Active native windows on Windows 10 may have a custom frame color.
    if (id == ThemeProperties::COLOR_FRAME)
      return dwm_frame_color_;

    if (id == ThemeProperties::COLOR_ACCENT_BORDER)
      return dwm_accent_border_color_;

    // Inactive native windows on Windows 10 always have a white frame.
    if (id == ThemeProperties::COLOR_FRAME_INACTIVE)
        return SK_ColorWHITE;
  }

  return ThemeService::GetDefaultColor(id, incognito);
}

bool ThemeServiceWin::ShouldUseDwmFrameColor() const {
  return ShouldUseNativeFrame() &&
         (base::win::GetVersion() >= base::win::VERSION_WIN10);
}

void ThemeServiceWin::OnDwmKeyUpdated() {
  // Attempt to read the accent color.
  DWORD accent_color, color_prevalence;
  dwm_frame_color_ =
      ((dwm_key_->ReadValueDW(L"ColorPrevalence", &color_prevalence) ==
        ERROR_SUCCESS) &&
       (color_prevalence == 1) &&
       (dwm_key_->ReadValueDW(L"AccentColor", &accent_color) == ERROR_SUCCESS))
          ? skia::COLORREFToSkColor(accent_color)
          : SK_ColorWHITE;

  dwm_accent_border_color_ = SK_ColorWHITE;
  DWORD colorization_color, colorization_color_balance;
  if ((dwm_key_->ReadValueDW(L"ColorizationColor", &colorization_color) ==
       ERROR_SUCCESS) &&
      (dwm_key_->ReadValueDW(L"ColorizationColorBalance",
                             &colorization_color_balance) == ERROR_SUCCESS)) {
    // The accent border color is a linear blend between the colorization
    // color and the neutral #d9d9d9. colorization_color_balance is the
    // percentage of the colorization color in that blend.
    //
    // On Windows version 1611 colorization_color_balance can be 0xfffffff3 if
    // the accent color is taken from the background and either the background
    // is a solid color or was just changed to a slideshow. It's unclear what
    // that value's supposed to mean, so change it to 80 to match Edge's
    // behavior.
    if (colorization_color_balance > 100)
      colorization_color_balance = 80;

    // colorization_color's high byte is not an alpha value, so replace it
    // with 0xff to make an opaque ARGB color.
    SkColor input_color = SkColorSetA(colorization_color, 0xff);

    dwm_accent_border_color_ = color_utils::AlphaBlend(
        input_color, SkColorSetRGB(0xd9, 0xd9, 0xd9),
        gfx::ToRoundedInt(255 * colorization_color_balance / 100.f));
  }

  // Watch for future changes.
  if (!dwm_key_->StartWatching(base::Bind(
          &ThemeServiceWin::OnDwmKeyUpdated, base::Unretained(this))))
    dwm_key_.reset();
}
