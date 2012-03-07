// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"

#include "base/logging.h"
#include "ui/gfx/screen.h"

DisplaySettingsProvider::DisplaySettingsProvider(Observer* observer)
    : observer_(observer) {
  DCHECK(observer);
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
}

gfx::Rect DisplaySettingsProvider::GetWorkArea() {
#if defined(OS_MACOSX)
  // On OSX, panels should be dropped all the way to the bottom edge of the
  // screen (and overlap Dock).
  gfx::Rect work_area = gfx::Screen::GetPrimaryMonitorBounds();
#else
  gfx::Rect work_area = gfx::Screen::GetPrimaryMonitorWorkArea();
#endif

  if (work_area_ != work_area) {
    work_area_ = work_area;
    OnWorkAreaChanged();
  }

  return work_area;
}

void DisplaySettingsProvider::OnWorkAreaChanged() {
}

bool DisplaySettingsProvider::IsAutoHidingDesktopBarEnabled(
    DesktopBarAlignment alignment) {
  return false;
}

int DisplaySettingsProvider::GetDesktopBarThickness(
    DesktopBarAlignment alignment) const {
  return 0;
}

DisplaySettingsProvider::DesktopBarVisibility
DisplaySettingsProvider::GetDesktopBarVisibility(
    DesktopBarAlignment alignment) const {
  return DESKTOP_BAR_VISIBLE;
}

#if !defined(OS_WIN)
// static
DisplaySettingsProvider* DisplaySettingsProvider::Create(Observer* observer) {
  return new DisplaySettingsProvider(observer);
}
#endif
