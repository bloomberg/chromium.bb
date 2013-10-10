// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/screen.h"

void WindowSizer::GetTabbedBrowserBoundsAsh(
    gfx::Rect* bounds_in_screen,
    ui::WindowShowState* show_state) const {
  DCHECK(show_state);
  DCHECK(bounds_in_screen);
  DCHECK(browser_->is_type_tabbed());
  DCHECK(bounds_in_screen->IsEmpty());

  ui::WindowShowState passed_show_state = *show_state;

  bool is_saved_bounds = GetSavedWindowBounds(bounds_in_screen, show_state);
  // If there is no saved bounds (hence bounds_in_screen is empty), the
  // |display| will be the primary display.
  const gfx::Display& display = screen_->GetDisplayMatching(*bounds_in_screen);
  if (!is_saved_bounds)
    *bounds_in_screen = ash::WindowPositioner::GetDefaultWindowBounds(display);

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
      screen_,
      browser_window,
      is_saved_bounds,
      passed_show_state,
      bounds_in_screen,
      show_state);
}
