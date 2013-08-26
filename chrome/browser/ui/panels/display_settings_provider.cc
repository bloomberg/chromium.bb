// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider.h"

#include "base/bind.h"
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

void DisplaySettingsProvider::AddDisplayObserver(DisplayObserver* observer) {
  display_observers_.AddObserver(observer);
}

void DisplaySettingsProvider::RemoveDisplayObserver(DisplayObserver* observer) {
  display_observers_.RemoveObserver(observer);
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
  is_full_screen_ = IsFullScreen();
  bool already_started = full_screen_observers_.might_have_observers();
  full_screen_observers_.AddObserver(observer);

  if (!already_started && NeedsPeriodicFullScreenCheck()) {
    full_screen_mode_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kFullScreenModeCheckIntervalMs),
        base::Bind(&DisplaySettingsProvider::CheckFullScreenMode,
                   base::Unretained(this),
                   PERFORM_FULLSCREEN_CHECK));
  }
}

void DisplaySettingsProvider::RemoveFullScreenObserver(
    FullScreenObserver* observer) {
  full_screen_observers_.RemoveObserver(observer);

  if (!full_screen_observers_.might_have_observers())
    full_screen_mode_timer_.Stop();
}

// TODO(scottmg): This should be moved to ui/.
gfx::Rect DisplaySettingsProvider::GetPrimaryDisplayArea() const {
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().bounds();
}

gfx::Rect DisplaySettingsProvider::GetPrimaryWorkArea() const {
#if defined(OS_MACOSX)
  // On OSX, panels should be dropped all the way to the bottom edge of the
  // screen (and overlap Dock). And we also want to exclude the system menu
  // area. Note that the rect returned from gfx::Screen util functions is in
  // platform-independent screen coordinates with (0, 0) as the top-left corner.
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  gfx::Display display = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  gfx::Rect display_area = display.bounds();
  gfx::Rect work_area = display.work_area();
  int system_menu_height = work_area.y() - display_area.y();
  if (system_menu_height > 0) {
    display_area.set_y(display_area.y() + system_menu_height);
    display_area.set_height(display_area.height() - system_menu_height);
  }
  return display_area;
#else
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area();
#endif
}

gfx::Rect DisplaySettingsProvider::GetDisplayAreaMatching(
    const gfx::Rect& bounds) const {
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  return gfx::Screen::GetNativeScreen()->GetDisplayMatching(bounds).bounds();
}

gfx::Rect DisplaySettingsProvider::GetWorkAreaMatching(
    const gfx::Rect& bounds) const {
  // TODO(scottmg): NativeScreen is wrong. http://crbug.com/133312
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  gfx::Display display = screen->GetDisplayMatching(bounds);
  if (display.bounds() == screen->GetPrimaryDisplay().bounds())
    return GetPrimaryWorkArea();
  return display.work_area();
}

void DisplaySettingsProvider::OnDisplaySettingsChanged() {
  FOR_EACH_OBSERVER(DisplayObserver, display_observers_, OnDisplayChanged());
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

bool DisplaySettingsProvider::NeedsPeriodicFullScreenCheck() const {
  return true;
}

void DisplaySettingsProvider::CheckFullScreenMode(
    FullScreenCheckMode check_mode) {
  bool is_full_screen = false;
  switch (check_mode) {
    case ASSUME_FULLSCREEN_ON:
      is_full_screen = true;
      break;
    case ASSUME_FULLSCREEN_OFF:
      is_full_screen = false;
      break;
    case PERFORM_FULLSCREEN_CHECK:
      is_full_screen = IsFullScreen();
      break;
    default:
      NOTREACHED();
      break;
  }
  if (is_full_screen == is_full_screen_)
    return;
  is_full_screen_ = is_full_screen;

  FOR_EACH_OBSERVER(FullScreenObserver,
                    full_screen_observers_,
                    OnFullScreenModeChanged(is_full_screen_));
}

bool DisplaySettingsProvider::IsFullScreen() {
  return IsFullScreenMode();
}

#if defined(USE_AURA)
// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProvider();
}
#endif
