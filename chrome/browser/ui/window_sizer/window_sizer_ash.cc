// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "ash/shell.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

bool WindowSizer::GetBrowserBoundsAsh(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  // TODO(crbug.com/764009): Mash support.
  if (ash_util::IsRunningInMash() || !browser_)
    return false;

  bool determined = false;
  if (bounds->IsEmpty()) {
    if (browser_->is_type_tabbed()) {
      GetTabbedBrowserBoundsAsh(bounds, show_state);
      determined = true;
    } else if (browser_->is_trusted_source()) {
      // For trusted popups (v1 apps and system windows), do not use the last
      // active window bounds, only use saved or default bounds.
      if (!GetSavedWindowBounds(bounds, show_state))
        *bounds = GetDefaultWindowBoundsAsh(GetTargetDisplay(gfx::Rect()));
      determined = true;
    } else {
      // In Ash, prioritize the last saved |show_state|. If you have questions
      // or comments about this behavior please contact oshima@chromium.org.
      if (state_provider_) {
        gfx::Rect ignored_bounds, ignored_work_area;
        state_provider_->GetPersistentState(&ignored_bounds,
                                            &ignored_work_area,
                                            show_state);
      }
    }
  } else if (browser_->is_type_popup() && bounds->origin().IsOrigin()) {
    // In case of a popup with an 'unspecified' location in ash, we are
    // looking for a good screen location. We are interpreting (0,0) as an
    // unspecified location.
    *bounds = ash::Shell::Get()->window_positioner()->GetPopupPosition(
        bounds->size());
    determined = true;
  }

  if (browser_->is_type_tabbed() && *show_state == ui::SHOW_STATE_DEFAULT) {
    display::Display display = screen_->GetDisplayMatching(*bounds);
    gfx::Rect work_area = display.work_area();
    bounds->AdjustToFit(work_area);
    if (*bounds == work_area) {
      // A |browser_| that occupies the whole work area gets maximized.
      // |bounds| returned here become the restore bounds once the window
      // gets maximized after this method returns. Return a sensible default
      // in order to make restored state visibly different from maximized.
      *show_state = ui::SHOW_STATE_MAXIMIZED;
      *bounds = GetDefaultWindowBoundsAsh(display);
      determined = true;
    }
  }
  return determined;
}

void WindowSizer::GetTabbedBrowserBoundsAsh(
    gfx::Rect* bounds_in_screen,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser_->is_type_tabbed());
  DCHECK(bounds_in_screen->IsEmpty());

  ui::WindowShowState passed_show_state = *show_state;

  bool is_saved_bounds = GetSavedWindowBounds(bounds_in_screen, show_state);
  display::Display display;
  if (is_saved_bounds) {
    display = screen_->GetDisplayMatching(*bounds_in_screen);
  } else {
    // If there is no saved bounds (hence bounds_in_screen is empty), use the
    // target display.
    display = target_display_provider_->GetTargetDisplay(screen_,
                                                         *bounds_in_screen);
    *bounds_in_screen = GetDefaultWindowBoundsAsh(display);
  }

  if (browser_->is_session_restore()) {
    // This is a fall-through case when there is no bounds recorded
    // for restored window, and should not be used except for the case
    // above.  The regular path is handled in
    // |WindowSizer::DetermineWindowBoundsAndShowState|.

    // Note: How restore bounds/show state data are passed.
    // The restore bounds is passed via |Browser::override_bounds()| in
    // |chrome::GetBrowserWindowBoundsAndShowState()|.
    // The restore state is passed via |Browser::initial_state()| in
    // |WindowSizer::GetWindowDefaultShowState|.
    bounds_in_screen->AdjustToFit(display.work_area());
    return;
  }

  // The |browser_window| is non NULL when this is called after
  // browser's aura window is created.
  aura::Window* browser_window =
      browser_->window() ? browser_->window()->GetNativeWindow() : NULL;

  ash::WindowPositioner::GetBoundsAndShowStateForNewWindow(
      browser_window, is_saved_bounds, passed_show_state, bounds_in_screen,
      show_state);
}

gfx::Rect WindowSizer::GetDefaultWindowBoundsAsh(
    const display::Display& display) {
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
  return gfx::Rect(work_area.x() + offset_x, work_area.y() + kDesktopBorderSize,
                   default_width, default_height);
}
