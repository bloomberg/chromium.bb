// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"

#include "base/logging.h"
#include "chrome/browser/fullscreen.h"
#include "ui/gfx/screen.h"

namespace {
// The polling interval to check any display settings change, like full-screen
// mode changes.
const int kFullScreenModeCheckIntervalMs = 1000;
}  // namespace

DisplaySettingsProvider::DisplaySettingsProvider()
    : is_full_screen_(false) {
}

DisplaySettingsProvider::~DisplaySettingsProvider() {
}

void DisplaySettingsProvider::AddDisplayAreaObserver(
    DisplayAreaObserver* observer) {
  display_area_observers_.AddObserver(observer);
}

void DisplaySettingsProvider::RemoveDisplayAreaObserver(
    DisplayAreaObserver* observer) {
  display_area_observers_.RemoveObserver(observer);
}

void DisplaySettingsProvider::AddDesktopBarObserver(
    DesktopBarObserver* observer) {
  desktop_bar_observers_.AddObserver(observer);
}

void DisplaySettingsProvider::RemoveDesktopBarObserver(
    DesktopBarObserver* observer) {
  desktop_bar_observers_.RemoveObserver(observer);
}

void DisplaySettingsProvider::AddFullScreenObserver(
    FullScreenObserver* observer) {
  full_screen_observers_.AddObserver(observer);

  if (full_screen_observers_.size() == 1) {
    full_screen_mode_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kFullScreenModeCheckIntervalMs),
        this, &DisplaySettingsProvider::CheckFullScreenMode);
  }
}

void DisplaySettingsProvider::RemoveFullScreenObserver(
    FullScreenObserver* observer) {
  full_screen_observers_.RemoveObserver(observer);

  if (full_screen_observers_.size() == 0)
    full_screen_mode_timer_.Stop();
}

gfx::Rect DisplaySettingsProvider::GetDisplayArea() {
  // Do the first-time initialization if not yet.
  if (adjusted_work_area_.IsEmpty())
    OnDisplaySettingsChanged();

  return adjusted_work_area_;
}

gfx::Rect DisplaySettingsProvider::GetPrimaryScreenArea() const {
  return gfx::Screen::GetPrimaryMonitor().bounds();
}

gfx::Rect DisplaySettingsProvider::GetWorkArea() const {
#if defined(OS_MACOSX)
  // On OSX, panels should be dropped all the way to the bottom edge of the
  // screen (and overlap Dock). And we also want to exclude the system menu
  // area. Note that the rect returned from gfx::Screen util functions is in
  // platform-independent screen coordinates with (0, 0) as the top-left corner.
  gfx::Monitor monitor = gfx::Screen::GetPrimaryMonitor();
  gfx::Rect monitor_area = monitor.bounds();
  gfx::Rect work_area = monitor.work_area();
  int system_menu_height = work_area.y() - monitor_area.y();
  if (system_menu_height > 0) {
    monitor_area.set_y(monitor_area.y() + system_menu_height);
    monitor_area.set_height(monitor_area.height() - system_menu_height);
  }
  return monitor_area;
#else
  gfx::Rect work_area = gfx::Screen::GetPrimaryMonitor().work_area();
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

  if (old_adjusted_work_area != adjusted_work_area_) {
    FOR_EACH_OBSERVER(DisplayAreaObserver,
                      display_area_observers_,
                      OnDisplayAreaChanged(adjusted_work_area_));
  }
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

void DisplaySettingsProvider::CheckFullScreenMode() {
  bool is_full_screen = IsFullScreenMode();
  if (is_full_screen == is_full_screen_)
    return;
  is_full_screen_ = is_full_screen;

  FOR_EACH_OBSERVER(FullScreenObserver,
                    full_screen_observers_,
                    OnFullScreenModeChanged(is_full_screen_));
}

#if defined(USE_AURA)
// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProvider();
}
#endif
