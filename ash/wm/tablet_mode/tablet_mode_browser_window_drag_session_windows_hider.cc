// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_session_windows_hider.h"

#include <vector>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

TabletModeBrowserWindowDragSessionWindowsHider::
    TabletModeBrowserWindowDragSessionWindowsHider(aura::Window* source_window,
                                                   aura::Window* dragged_window)
    : source_window_(source_window), dragged_window_(dragged_window) {
  DCHECK(source_window_);
  if (dragged_window_) {
    DCHECK_EQ(source_window_,
              dragged_window_->GetProperty(kTabDraggingSourceWindowKey));
  }

  root_window_ = source_window_->GetRootWindow();

  // Disable the backdrop for |source_window_| during dragging.
  WindowBackdrop::Get(source_window_)->DisableBackdrop();

  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

  std::vector<aura::Window*> windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (aura::Window* window : windows) {
    if (window == dragged_window_ || window == source_window_ ||
        window->GetRootWindow() != root_window_) {
      continue;
    }

    window_visibility_map_.emplace(window, window->IsVisible());
    if (window->IsVisible()) {
      ScopedAnimationDisabler disabler(window);
      window->Hide();
    }
    window->AddObserver(this);
  }

  // Hide the home launcher if it's enabled during dragging.
  Shell::Get()->home_screen_controller()->OnWindowDragStarted();

  // Blurs the wallpaper background.
  RootWindowController::ForWindow(root_window_)
      ->wallpaper_widget_controller()
      ->SetWallpaperProperty(wallpaper_constants::kOverviewState);
}

TabletModeBrowserWindowDragSessionWindowsHider::
    ~TabletModeBrowserWindowDragSessionWindowsHider() {
  // It might be possible that |source_window_| is destroyed during dragging.
  if (source_window_)
    WindowBackdrop::Get(source_window_)->RestoreBackdrop();

  for (auto iter = window_visibility_map_.begin();
       iter != window_visibility_map_.end(); ++iter) {
    iter->first->RemoveObserver(this);
    if (iter->second) {
      ScopedAnimationDisabler disabler(iter->first);
      iter->first->Show();
    }
  }

  DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());

  // May reshow the home launcher after dragging.
  Shell::Get()->home_screen_controller()->OnWindowDragEnded(
      /*animate=*/false);

  // Clears the background wallpaper blur.
  RootWindowController::ForWindow(root_window_)
      ->wallpaper_widget_controller()
      ->SetWallpaperProperty(wallpaper_constants::kClear);
}

void TabletModeBrowserWindowDragSessionWindowsHider::OnWindowDestroying(
    aura::Window* window) {
  if (window == source_window_) {
    source_window_ = nullptr;
    return;
  }

  window->RemoveObserver(this);
  window_visibility_map_.erase(window);
}
void TabletModeBrowserWindowDragSessionWindowsHider::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (window == source_window_)
    return;

  if (visible) {
    // Do not let |window| change to visible during the lifetime of |this|.
    // Also update |window_visibility_map_| so that we can restore the window
    // visibility correctly.
    window->Hide();
    window_visibility_map_[window] = visible;
  }
  // else do nothing. It must come from Hide() function above thus should be
  // ignored.
}

}  // namespace ash
