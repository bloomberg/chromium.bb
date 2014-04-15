// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer/window_sizer.h"

#include "ash/shell.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_state.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/screen.h"

bool WindowSizer::GetBrowserBoundsAsh(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  if (!browser_ ||
      browser_->host_desktop_type() != chrome::HOST_DESKTOP_TYPE_ASH)
    return false;

  if (bounds->IsEmpty()) {
    if (browser_->is_type_tabbed()) {
      GetTabbedBrowserBoundsAsh(bounds, show_state);
      return true;
    }

    if (browser_->is_trusted_source()) {
      // For trusted popups (v1 apps and system windows), do not use the last
      // active window bounds, only use saved or default bounds.
      if (!GetSavedWindowBounds(bounds, show_state))
        GetDefaultWindowBounds(GetTargetDisplay(gfx::Rect()), bounds);
      return true;
    }

    // In Ash, prioritize the last saved |show_state|. If you have questions
    // or comments about this behavior please contact oshima@chromium.org.
    if (state_provider_) {
      gfx::Rect ignored_bounds, ignored_work_area;
      state_provider_->GetPersistentState(&ignored_bounds,
                                          &ignored_work_area,
                                          show_state);
    }
    return false;
  }

  // In case of a popup with an 'unspecified' location in ash, we are
  // looking for a good screen location. We are interpreting (0,0) as an
  // unspecified location.
  if (browser_->is_type_popup() && bounds->origin().IsOrigin()) {
    *bounds = ash::Shell::GetInstance()->window_positioner()->
        GetPopupPosition(*bounds);
    return true;
  }
  return false;
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
  gfx::Display display;
  if (is_saved_bounds) {
    display = screen_->GetDisplayMatching(*bounds_in_screen);
  } else {
    // If there is no saved bounds (hence bounds_in_screen is empty), use the
    // target display.
    display = target_display_provider_->GetTargetDisplay(screen_,
                                                         *bounds_in_screen);
    *bounds_in_screen = ash::WindowPositioner::GetDefaultWindowBounds(display);
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
      screen_,
      browser_window,
      is_saved_bounds,
      passed_show_state,
      bounds_in_screen,
      show_state);
}
