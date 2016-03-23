// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_service_win.h"

#include "base/bind.h"
#include "base/win/windows_version.h"
#include "chrome/browser/themes/theme_properties.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/win/shell.h"

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
  return !HasCustomImage(IDR_THEME_FRAME) && ui::win::IsAeroGlassEnabled();
}

SkColor ThemeServiceWin::GetDefaultColor(int id, bool incognito) const {
  if (ShouldUseDwmFrameColor()) {
    // Active native windows on Windows 10 may have a custom frame color.
    if (id == ThemeProperties::COLOR_FRAME)
      return dwm_frame_color_;

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

  // Watch for future changes.
  if (!dwm_key_->StartWatching(base::Bind(
          &ThemeServiceWin::OnDwmKeyUpdated, base::Unretained(this))))
    dwm_key_.reset();
}
