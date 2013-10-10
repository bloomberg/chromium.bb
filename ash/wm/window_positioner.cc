// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioner.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/auto_window_management.h"
#include "base/command_line.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/screen.h"

namespace ash {

// When a window gets opened in default mode and the screen is less than or
// equal to this width, the window will get opened in maximized mode. This value
// can be reduced to a "tame" number if the feature is disabled.
const int kForceMaximizeWidthLimit = 1366;
const int kForceMaximizeWidthLimitDisabled = 640;

const int WindowPositioner::kMinimumWindowOffset = 32;

// The number of pixels which are kept free top, left and right when a window
// gets positioned to its default location.
// static
const int WindowPositioner::kDesktopBorderSize = 16;

// Maximum width of a window even if there is more room on the desktop.
// static
const int WindowPositioner::kMaximumWindowWidth = 1100;

// static
int WindowPositioner::GetForceMaximizedWidthLimit() {
  static int maximum_limit = 0;
  if (!maximum_limit) {
    maximum_limit = CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kAshDisableAutoMaximizing) ?
        kForceMaximizeWidthLimitDisabled : kForceMaximizeWidthLimit;
  }
  return maximum_limit;
}

void WindowPositioner::GetBoundsAndShowStateForNewWindow(
    const gfx::Screen* screen,
    const aura::Window* new_window,
    bool is_saved_bounds,
    ui::WindowShowState show_state_in,
    gfx::Rect* bounds_in_out,
    ui::WindowShowState* show_state_out) {

  // Always open new window in the target display.
  aura::RootWindow* target = Shell::GetTargetRootWindow();

  aura::Window* top_window = GetTopWindowForNewWindow(target);
  // Our window should not have any impact if we are already on top.
  if (top_window == new_window)
    top_window = NULL;

  // If there is no valid other window we take the coordinates as is.
  if (!top_window) {
    gfx::Rect work_area = screen->GetDisplayNearestWindow(target).work_area();

    if (is_saved_bounds) {
      // Restore to saved state - if there is one.
      bounds_in_out->AdjustToFit(work_area);
      return;
    }

    // When using "small screens" we want to always open in full screen mode.
    if (show_state_in == ui::SHOW_STATE_DEFAULT &&
        work_area.width() <=
        WindowPositioner::GetForceMaximizedWidthLimit() &&
        (!new_window || !wm::GetWindowState(new_window)->IsFullscreen())) {
      *show_state_out = ui::SHOW_STATE_MAXIMIZED;
    }
    return;
  }
  bool maximized = wm::GetWindowState(top_window)->IsMaximized();
  // We ignore the saved show state, but look instead for the top level
  // window's show state.
  if (show_state_in == ui::SHOW_STATE_DEFAULT) {
    *show_state_out = maximized ? ui::SHOW_STATE_MAXIMIZED :
        ui::SHOW_STATE_DEFAULT;
  }

  // Use the size of the other window. The window's bound will be rearranged
  // in ash::WorkspaceLayoutManager using this location.
  *bounds_in_out = top_window->GetBoundsInScreen();
}

WindowPositioner::WindowPositioner()
    : pop_position_offset_increment_x(0),
      pop_position_offset_increment_y(0),
      popup_position_offset_from_screen_corner_x(0),
      popup_position_offset_from_screen_corner_y(0),
      last_popup_position_x_(0),
      last_popup_position_y_(0) {
}

WindowPositioner::~WindowPositioner() {
}

gfx::Rect WindowPositioner::GetDefaultWindowBounds(
    const gfx::Display& display) {
  const gfx::Rect work_area = display.work_area();
  // There should be a 'desktop' border around the window at the left and right
  // side.
  int default_width = work_area.width() - 2 * kDesktopBorderSize;
  // There should also be a 'desktop' border around the window at the top.
  // Since the workspace excludes the tray area we only need one border size.
  int default_height = work_area.height() - kDesktopBorderSize;
  int offset_x = kDesktopBorderSize;
  if (default_width > kMaximumWindowWidth) {
    // The window should get centered on the screen and not follow the grid.
    offset_x = (work_area.width() - kMaximumWindowWidth) / 2;
    default_width = kMaximumWindowWidth;
  }
  return gfx::Rect(work_area.x() + offset_x,
                   work_area.y() + kDesktopBorderSize,
                   default_width,
                   default_height);
}

gfx::Rect WindowPositioner::GetPopupPosition(const gfx::Rect& old_pos) {
  int grid = kMinimumWindowOffset;
  popup_position_offset_from_screen_corner_x = grid;
  popup_position_offset_from_screen_corner_y = grid;
  if (!pop_position_offset_increment_x) {
    // When the popup position increment is , the last popup position
    // was not yet initialized.
    last_popup_position_x_ = popup_position_offset_from_screen_corner_x;
    last_popup_position_y_ = popup_position_offset_from_screen_corner_y;
  }
  pop_position_offset_increment_x = grid;
  pop_position_offset_increment_y = grid;
  // We handle the Multi monitor support by retrieving the active window's
  // work area.
  aura::Window* window = wm::GetActiveWindow();
  const gfx::Rect work_area = window && window->IsVisible() ?
      Shell::GetScreen()->GetDisplayNearestWindow(window).work_area() :
      Shell::GetScreen()->GetPrimaryDisplay().work_area();
  // Only try to reposition the popup when it is not spanning the entire
  // screen.
  if ((old_pos.width() + popup_position_offset_from_screen_corner_x >=
      work_area.width()) ||
      (old_pos.height() + popup_position_offset_from_screen_corner_y >=
       work_area.height()))
    return AlignPopupPosition(old_pos, work_area, grid);
  const gfx::Rect result = SmartPopupPosition(old_pos, work_area, grid);
  if (!result.IsEmpty())
    return AlignPopupPosition(result, work_area, grid);
  return NormalPopupPosition(old_pos, work_area);
}

gfx::Rect WindowPositioner::NormalPopupPosition(
    const gfx::Rect& old_pos,
    const gfx::Rect& work_area) {
  int w = old_pos.width();
  int h = old_pos.height();
  // Note: The 'last_popup_position' is checked and kept relative to the
  // screen size. The offsetting will be done in the last step when the
  // target rectangle gets returned.
  bool reset = false;
  if (last_popup_position_y_ + h > work_area.height() ||
      last_popup_position_x_ + w > work_area.width()) {
    // Popup does not fit on screen. Reset to next diagonal row.
    last_popup_position_x_ -= last_popup_position_y_ -
                              popup_position_offset_from_screen_corner_x -
                              pop_position_offset_increment_x;
    last_popup_position_y_ = popup_position_offset_from_screen_corner_y;
    reset = true;
  }
  if (last_popup_position_x_ + w > work_area.width()) {
    // Start again over.
    last_popup_position_x_ = popup_position_offset_from_screen_corner_x;
    last_popup_position_y_ = popup_position_offset_from_screen_corner_y;
    reset = true;
  }
  int x = last_popup_position_x_;
  int y = last_popup_position_y_;
  if (!reset) {
    last_popup_position_x_ += pop_position_offset_increment_x;
    last_popup_position_y_ += pop_position_offset_increment_y;
  }
  return gfx::Rect(x + work_area.x(), y + work_area.y(), w, h);
}

gfx::Rect WindowPositioner::SmartPopupPosition(
    const gfx::Rect& old_pos,
    const gfx::Rect& work_area,
    int grid) {
  const std::vector<aura::Window*> windows =
      MruWindowTracker::BuildWindowList(false);

  std::vector<const gfx::Rect*> regions;
  // Process the window list and check if we can bail immediately.
  for (size_t i = 0; i < windows.size(); i++) {
    // We only include opaque and visible windows.
    if (windows[i] && windows[i]->IsVisible() && windows[i]->layer() &&
        (!windows[i]->transparent() ||
         windows[i]->layer()->GetTargetOpacity() == 1.0)) {
      wm::WindowState* window_state = wm::GetWindowState(windows[i]);
      // When any window is maximized we cannot find any free space.
      if (window_state->IsMaximizedOrFullscreen())
        return gfx::Rect(0, 0, 0, 0);
      if (window_state->IsNormalShowState())
        regions.push_back(&windows[i]->bounds());
    }
  }

  if (regions.empty())
    return gfx::Rect(0, 0, 0, 0);

  int w = old_pos.width();
  int h = old_pos.height();
  int x_end = work_area.width() / 2;
  int x, x_increment;
  // We parse for a proper location on the screen. We do this in two runs:
  // The first run will start from the left, parsing down, skipping any
  // overlapping windows it will encounter until the popup's height can not
  // be served anymore. Then the next grid position to the right will be
  // taken, and the same cycle starts again. This will be repeated until we
  // hit the middle of the screen (or we find a suitable location).
  // In the second run we parse beginning from the right corner downwards and
  // then to the left.
  // When no location was found, an empty rectangle will be returned.
  for (int run = 0; run < 2; run++) {
    if (run == 0) { // First run: Start left, parse right till mid screen.
      x = 0;
      x_increment = pop_position_offset_increment_x;
    } else { // Second run: Start right, parse left till mid screen.
      x = work_area.width() - w;
      x_increment = -pop_position_offset_increment_x;
    }
    // Note: The passing (x,y,w,h) window is always relative to the work area's
    // origin.
    for (; x_increment > 0 ? (x < x_end) : (x > x_end); x += x_increment) {
      int y = 0;
      while (y + h <= work_area.height()) {
        size_t i;
        for (i = 0; i < regions.size(); i++) {
          if (regions[i]->Intersects(gfx::Rect(x + work_area.x(),
                                               y + work_area.y(), w, h))) {
            y = regions[i]->bottom() - work_area.y();
            break;
          }
        }
        if (i >= regions.size())
          return gfx::Rect(x + work_area.x(), y + work_area.y(), w, h);
      }
    }
  }
  return gfx::Rect(0, 0, 0, 0);
}

gfx::Rect WindowPositioner::AlignPopupPosition(
    const gfx::Rect& pos,
    const gfx::Rect& work_area,
    int grid) {
  if (grid <= 1)
    return pos;

  int x = pos.x() - (pos.x() - work_area.x()) % grid;
  int y = pos.y() - (pos.y() - work_area.y()) % grid;
  int w = pos.width();
  int h = pos.height();

  // If the alignment was pushing the window out of the screen, we ignore the
  // alignment for that call.
  if (abs(pos.right() - work_area.right()) < grid)
    x = work_area.right() - w;
  if (abs(pos.bottom() - work_area.bottom()) < grid)
    y = work_area.bottom() - h;
  return gfx::Rect(x, y, w, h);
}

}  // namespace ash
