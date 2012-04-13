// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"

#include "base/logging.h"
#include "ui/gfx/screen.h"

DisplaySettingsProvider::DisplaySettingsProvider()
    : display_area_observer_(NULL),
      desktop_bar_observer_(NULL) {
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
}

gfx::Rect DisplaySettingsProvider::GetDisplayArea() {
  // Do the first-time initialization if not yet.
  if (adjusted_work_area_.IsEmpty())
    OnDisplaySettingsChanged();

  return adjusted_work_area_;
}

gfx::Rect DisplaySettingsProvider::GetWorkArea() const {
#if defined(OS_MACOSX)
  // On OSX, panels should be dropped all the way to the bottom edge of the
  // screen (and overlap Dock).
  gfx::Rect work_area = gfx::Screen::GetPrimaryMonitorBounds();
#else
  gfx::Rect work_area = gfx::Screen::GetPrimaryMonitorWorkArea();
#endif
  return work_area;
}

void DisplaySettingsProvider::OnDisplaySettingsChanged() {
  gfx::Rect work_area = GetWorkArea();
  if (work_area == work_area_)
    return;
  work_area_ = work_area;

  OnAutoHidingDesktopBarChanged();
}

void DisplaySettingsProvider::OnAutoHidingDesktopBarChanged() {
  gfx::Rect old_adjusted_work_area = adjusted_work_area_;
  AdjustWorkAreaForAutoHidingDesktopBars();

  if (old_adjusted_work_area != adjusted_work_area_ && display_area_observer_)
    display_area_observer_->OnDisplayAreaChanged(adjusted_work_area_);
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

void DisplaySettingsProvider::AdjustWorkAreaForAutoHidingDesktopBars() {
  // Note that we do not care about the top desktop bar since panels could not
  // reach so high due to size constraint. We also do not care about the bottom
  // desktop bar since we always align the panel to the bottom of the work area.
  adjusted_work_area_ = work_area_;
  if (IsAutoHidingDesktopBarEnabled(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT)) {
    int space = GetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_LEFT);
    adjusted_work_area_.set_x(adjusted_work_area_.x() + space);
    adjusted_work_area_.set_width(adjusted_work_area_.width() - space);
  }
  if (IsAutoHidingDesktopBarEnabled(
      DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT)) {
    int space = GetDesktopBarThickness(
        DisplaySettingsProvider::DESKTOP_BAR_ALIGNED_RIGHT);
    adjusted_work_area_.set_width(adjusted_work_area_.width() - space);
  }
}

#if defined(USE_AURA)
// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProvider();
}
#endif
