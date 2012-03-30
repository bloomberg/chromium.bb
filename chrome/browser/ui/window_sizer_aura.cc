// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/window_util.h"
#include "base/compiler_specific.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/screen.h"

namespace {

// Check if the window was not created as popup or as panel.
bool IsValidToplevelWindow(aura::Window* window) {
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    Browser* browser = *iter;
    if (browser && browser->window()->GetNativeHandle() == window) {
      return (!(browser->is_type_popup() || browser->is_type_panel()));
    }
  }
  // A window which has no browser associated with it is probably not a window
  // of which we want to copy the size from.
  return false;
}

// Get the first open window in the stack on the screen.
aura::Window* GetTopWindow() {
  // Get the active window.
  aura::Window* window = ash::wm::GetActiveWindow();
  if (window && window->type() == aura::client::WINDOW_TYPE_NORMAL &&
      window->IsVisible() && IsValidToplevelWindow(window))
    return window;

  // Get a list of all windows.
  const std::vector<aura::Window*>& windows =
      ash::Shell::GetInstance()->delegate()->GetCycleWindowList(
          ash::ShellDelegate::SOURCE_KEYBOARD);

  if (windows.empty())
    return NULL;

  aura::Window::Windows::const_iterator iter = windows.begin();
  // Find the index of the current window.
  if (window)
    iter = std::find(windows.begin(), windows.end(), window);

  int index = (iter == windows.end()) ? 0 : (iter - windows.begin());

  // Scan the cycle list backwards to see which is the second topmost window
  // (and so on). Note that we might cycle a few indices twice if there is no
  // suitable window. However - since the list is fairly small this should be
  // very fast anyways.
  for (int i = index + windows.size(); i >= 0; i--) {
    aura::Window* window = windows[i % windows.size()];
    if (window && window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        window->IsVisible() && IsValidToplevelWindow(window))
      return window;
  }
  return NULL;
}

}  // namespace

// This doesn't matter for aura, which has different tiling.
// static
const int WindowSizer::kWindowTilePixels = 10;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  // TODO(oshima):This is used to control panel/popups, and this may not be
  // needed on aura environment as they must be controlled by WM.
  return gfx::Point();
}

bool WindowSizer::GetBoundsIgnoringPreviousState(
    const gfx::Rect& specified_bounds,
    gfx::Rect* bounds) const {
  *bounds = specified_bounds;
  DCHECK(bounds->IsEmpty());
  if (browser_ != NULL && browser_->type() == Browser::TYPE_TABBED) {
    // This is a window / app. See if there is no window and try to place it.
    aura::Window* top_window = GetTopWindow();
    // If there are no windows we have a special case and try to
    // maximize which leaves a 'border' which shows the desktop.
    if (top_window == NULL) {
      GetDefaultWindowBounds(bounds);
    } else {
      *bounds = top_window->GetScreenBounds();
    }
    return true;
    // If both fail we will continue the default path.
  }

  return false;
}

void WindowSizer::GetDefaultWindowBounds(gfx::Rect* default_bounds) const {
  DCHECK(default_bounds);
  DCHECK(monitor_info_provider_.get());

  gfx::Rect work_area = monitor_info_provider_->GetPrimaryMonitorWorkArea();

  DCHECK_EQ(16, ash::Shell::GetInstance()->GetGridSize());
  int border = 16;

  // There should be a 'desktop' border around the window at the left and right
  // side.
  int default_width = work_area.width() - 2 * border;
  // There should also be a 'desktop' border around the window at the top.
  // Since the workspace excludes the tray area we only need one border size.
  int default_height = work_area.height() - border;
  if (default_width > 1280) {
    // Never make a window wider then 1280.
    default_width = 1280;
  }
  default_bounds->SetRect(work_area.x() + border,
                          work_area.y() + border,
                          default_width,
                          default_height);
}
