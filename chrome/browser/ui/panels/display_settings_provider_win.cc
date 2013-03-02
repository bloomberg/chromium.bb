// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/display_settings_provider_win.h"

#include <shellapi.h>

#include "base/logging.h"
#include "ui/views/widget/monitor_win.h"

namespace {
// The thickness of the area of an auto-hiding taskbar that is still visible
// when the taskbar becomes hidden.
const int kHiddenAutoHideTaskbarThickness = 2;

// The polling interval to check auto-hiding taskbars.
const int kCheckTaskbarPollingIntervalMs = 500;
}  // namespace

DisplaySettingsProviderWin::DisplaySettingsProviderWin()
    : monitor_(NULL) {
  memset(taskbars_, 0, sizeof(taskbars_));
}

DisplaySettingsProviderWin::~DisplaySettingsProviderWin() {
}

void DisplaySettingsProviderWin::OnDisplaySettingsChanged() {
  DisplaySettingsProvider::OnDisplaySettingsChanged();

  gfx::Rect primary_work_area = GetPrimaryWorkArea();
  RECT rect = primary_work_area.ToRECT();
  monitor_ = ::MonitorFromRect(&rect, MONITOR_DEFAULTTOPRIMARY);
  DCHECK(monitor_);

  bool taskbar_exists = CheckTaskbars(false);

  // If no auto-hiding taskbar exists, we do not need to start the polling
  // timer. If a taskbar is then set to auto-hiding, UpdateWorkArea will be
  // called due to the work area change.
  if (taskbar_exists) {
    if (!polling_timer_.IsRunning()) {
      polling_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kCheckTaskbarPollingIntervalMs),
          this,
          &DisplaySettingsProviderWin::OnPollingTimer);
    }
  } else {
    if (polling_timer_.IsRunning())
      polling_timer_.Stop();
  }
}

bool DisplaySettingsProviderWin::IsAutoHidingDesktopBarEnabled(
    DesktopBarAlignment alignment) {
  CheckTaskbars(false);
  return taskbars_[static_cast<int>(alignment)].window != NULL;
}

int DisplaySettingsProviderWin::GetDesktopBarThickness(
    DesktopBarAlignment alignment) const {
  return GetDesktopBarThicknessFromBounds(alignment, GetBounds(alignment));
}

DisplaySettingsProvider::DesktopBarVisibility
DisplaySettingsProviderWin::GetDesktopBarVisibility(
    DesktopBarAlignment alignment) const {
  return GetDesktopBarVisibilityFromBounds(alignment, GetBounds(alignment));
}

gfx::Rect DisplaySettingsProviderWin::GetBounds(
    DesktopBarAlignment alignment) const {
  HWND taskbar_window = taskbars_[static_cast<int>(alignment)].window;
  if (!taskbar_window)
    return gfx::Rect();

  RECT rect;
  if (!::GetWindowRect(taskbar_window, &rect))
    return gfx::Rect();
  return gfx::Rect(rect);
}

int DisplaySettingsProviderWin::GetDesktopBarThicknessFromBounds(
    DesktopBarAlignment alignment,
    const gfx::Rect& taskbar_bounds) const {
  switch (alignment) {
    case DESKTOP_BAR_ALIGNED_BOTTOM:
      return taskbar_bounds.height();
    case DESKTOP_BAR_ALIGNED_LEFT:
    case DESKTOP_BAR_ALIGNED_RIGHT:
      return taskbar_bounds.width();
    default:
      NOTREACHED();
      return 0;
  }
}

DisplaySettingsProvider::DesktopBarVisibility
DisplaySettingsProviderWin::GetDesktopBarVisibilityFromBounds(
    DesktopBarAlignment alignment,
    const gfx::Rect& taskbar_bounds) const {
  gfx::Rect primary_work_area = GetPrimaryWorkArea();
  switch (alignment) {
    case DESKTOP_BAR_ALIGNED_BOTTOM:
      if (taskbar_bounds.bottom() <= primary_work_area.bottom())
        return DESKTOP_BAR_VISIBLE;
      else if (taskbar_bounds.y() >=
               primary_work_area.bottom() - kHiddenAutoHideTaskbarThickness)
        return DESKTOP_BAR_HIDDEN;
      else
        return DESKTOP_BAR_ANIMATING;

    case DESKTOP_BAR_ALIGNED_LEFT:
      if (taskbar_bounds.x() >= primary_work_area.x())
        return DESKTOP_BAR_VISIBLE;
      else if (taskbar_bounds.right() <=
               primary_work_area.x() + kHiddenAutoHideTaskbarThickness)
        return DESKTOP_BAR_HIDDEN;
      else
        return DESKTOP_BAR_ANIMATING;

    case DESKTOP_BAR_ALIGNED_RIGHT:
      if (taskbar_bounds.right() <= primary_work_area.right())
        return DESKTOP_BAR_VISIBLE;
      else if (taskbar_bounds.x() >=
               primary_work_area.right() - kHiddenAutoHideTaskbarThickness)
        return DESKTOP_BAR_HIDDEN;
      else
        return DESKTOP_BAR_ANIMATING;

    default:
      NOTREACHED();
      return DESKTOP_BAR_VISIBLE;
  }
}

void DisplaySettingsProviderWin::OnPollingTimer() {
  CheckTaskbars(true);
}

bool DisplaySettingsProviderWin::CheckTaskbars(bool notify_observer) {
  bool taskbar_exists = false;
  UINT edges[] = { ABE_BOTTOM };
  for (size_t i = 0; i < kMaxTaskbars; ++i) {
    taskbars_[i].window =
        views::GetTopmostAutoHideTaskbarForEdge(edges[i], monitor_);
    if (taskbars_[i].window)
      taskbar_exists = true;
  }
  if (!taskbar_exists) {
    for (size_t i = 0; i < kMaxTaskbars; ++i) {
      taskbars_[i].thickness = 0;
      taskbars_[i].visibility = DESKTOP_BAR_HIDDEN;
    }
    return false;
  }

  for (size_t i = 0; i < kMaxTaskbars; ++i) {
    DesktopBarAlignment alignment = static_cast<DesktopBarAlignment>(i);

    gfx::Rect bounds = GetBounds(alignment);

    // Check the thickness change.
    int thickness = GetDesktopBarThicknessFromBounds(alignment, bounds);
    if (thickness != taskbars_[i].thickness) {
      taskbars_[i].thickness = thickness;
      if (notify_observer) {
        FOR_EACH_OBSERVER(
            DesktopBarObserver,
            desktop_bar_observers(),
            OnAutoHidingDesktopBarThicknessChanged(alignment, thickness));
      }
    }

    // Check and notify the visibility change.
    DesktopBarVisibility visibility = GetDesktopBarVisibilityFromBounds(
        alignment, bounds);
    if (visibility != taskbars_[i].visibility) {
      taskbars_[i].visibility = visibility;
      if (notify_observer) {
        FOR_EACH_OBSERVER(
            DesktopBarObserver,
            desktop_bar_observers(),
            OnAutoHidingDesktopBarVisibilityChanged(alignment, visibility));
      }
    }
  }

  return true;
}

// static
DisplaySettingsProvider* DisplaySettingsProvider::Create() {
  return new DisplaySettingsProviderWin();
}
