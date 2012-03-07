// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/auto_hiding_desktop_bar_win.h"

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

AutoHidingDesktopBarWin::AutoHidingDesktopBarWin(Observer* observer)
    : observer_(observer),
      monitor_(NULL) {
  DCHECK(observer);
  memset(taskbars_, 0, sizeof(taskbars_));
}

AutoHidingDesktopBarWin::~AutoHidingDesktopBarWin() {
}

void AutoHidingDesktopBarWin::UpdateWorkArea(const gfx::Rect& work_area) {
  if (work_area_ == work_area)
    return;
  work_area_ = work_area;

  RECT rect = work_area_.ToRECT();
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
          &AutoHidingDesktopBarWin::OnPollingTimer);
    }
  } else {
    if (polling_timer_.IsRunning())
      polling_timer_.Stop();
  }
}

bool AutoHidingDesktopBarWin::IsEnabled(
    AutoHidingDesktopBar::Alignment alignment) {
  CheckTaskbars(false);
  return taskbars_[static_cast<int>(alignment)].window != NULL;
}

int AutoHidingDesktopBarWin::GetThickness(
    AutoHidingDesktopBar::Alignment alignment) const {
  return GetThicknessFromBounds(alignment, GetBounds(alignment));
}

AutoHidingDesktopBar::Visibility AutoHidingDesktopBarWin::GetVisibility(
    AutoHidingDesktopBar::Alignment alignment) const {
  return GetVisibilityFromBounds(alignment, GetBounds(alignment));
}

gfx::Rect AutoHidingDesktopBarWin::GetBounds(
    AutoHidingDesktopBar::Alignment alignment) const {
  HWND taskbar_window = taskbars_[static_cast<int>(alignment)].window;
  if (!taskbar_window)
    return gfx::Rect();

  RECT rect;
  if (!::GetWindowRect(taskbar_window, &rect))
    return gfx::Rect();
  return gfx::Rect(rect);
}

int AutoHidingDesktopBarWin::GetThicknessFromBounds(
    AutoHidingDesktopBar::Alignment alignment,
    const gfx::Rect& taskbar_bounds) const {
  switch (alignment) {
    case AutoHidingDesktopBar::ALIGN_BOTTOM:
      return taskbar_bounds.height();
    case AutoHidingDesktopBar::ALIGN_LEFT:
    case AutoHidingDesktopBar::ALIGN_RIGHT:
      return taskbar_bounds.width();
    default:
      NOTREACHED();
      return 0;
  }
}

AutoHidingDesktopBar::Visibility
AutoHidingDesktopBarWin::GetVisibilityFromBounds(
    AutoHidingDesktopBar::Alignment alignment,
    const gfx::Rect& taskbar_bounds) const {
  switch (alignment) {
    case AutoHidingDesktopBar::ALIGN_BOTTOM:
      if (taskbar_bounds.bottom() <= work_area_.bottom())
        return VISIBLE;
      else if (taskbar_bounds.y() >=
               work_area_.bottom() - kHiddenAutoHideTaskbarThickness)
        return HIDDEN;
      else
        return ANIMATING;

    case AutoHidingDesktopBar::ALIGN_LEFT:
      if (taskbar_bounds.x() >= work_area_.x())
        return VISIBLE;
      else if (taskbar_bounds.right() <=
               work_area_.x() + kHiddenAutoHideTaskbarThickness)
        return HIDDEN;
      else
        return ANIMATING;

    case AutoHidingDesktopBar::ALIGN_RIGHT:
      if (taskbar_bounds.right() <= work_area_.right())
        return VISIBLE;
      else if (taskbar_bounds.x() >=
               work_area_.right() - kHiddenAutoHideTaskbarThickness)
        return HIDDEN;
      else
        return ANIMATING;

    default:
      NOTREACHED();
      return VISIBLE;
  }
}

void AutoHidingDesktopBarWin::OnPollingTimer() {
  CheckTaskbars(true);
}

bool AutoHidingDesktopBarWin::CheckTaskbars(bool notify_observer) {
  bool taskbar_exists = false;
  UINT edges[] = { ABE_BOTTOM, ABE_LEFT, ABE_RIGHT };
  for (size_t i = 0; i < kMaxTaskbars; ++i) {
    taskbars_[i].window =
        views::GetTopmostAutoHideTaskbarForEdge(edges[i], monitor_);
    if (taskbars_[i].window)
      taskbar_exists = true;
  }
  if (!taskbar_exists) {
    for (size_t i = 0; i < kMaxTaskbars; ++i) {
      taskbars_[i].thickness = 0;
      taskbars_[i].visibility = AutoHidingDesktopBar::HIDDEN;
    }
    return false;
  }

  bool thickness_changed = false;
  for (size_t i = 0; i < kMaxTaskbars; ++i) {
    AutoHidingDesktopBar::Alignment alignment = static_cast<Alignment>(i);

    gfx::Rect bounds = GetBounds(alignment);

    // Check the thickness change.
    int thickness = GetThicknessFromBounds(alignment, bounds);
    if (thickness != taskbars_[i].thickness) {
      taskbars_[i].thickness = thickness;
      thickness_changed = true;
    }

    // Check and notify the visibility change.
    AutoHidingDesktopBar::Visibility visibility =
        GetVisibilityFromBounds(alignment, bounds);
    if (visibility != taskbars_[i].visibility) {
      taskbars_[i].visibility = visibility;
      if (notify_observer) {
        observer_->OnAutoHidingDesktopBarVisibilityChanged(alignment,
                                                           visibility);
      }
    }
  }

  // Notify the thickness change if needed.
  if (thickness_changed && notify_observer)
    observer_->OnAutoHidingDesktopBarThicknessChanged();

  return true;
}

// static
AutoHidingDesktopBar* AutoHidingDesktopBar::Create(Observer* observer) {
  return new AutoHidingDesktopBarWin(observer);
}
