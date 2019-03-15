// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/logging.h"
#include "ui/aura/window.h"

namespace ash {

HomeScreenController::HomeScreenController()
    : home_launcher_gesture_handler_(
          std::make_unique<HomeLauncherGestureHandler>()) {
  wallpaper_controller_observer_.Add(Shell::Get()->wallpaper_controller());
}

HomeScreenController::~HomeScreenController() = default;

bool HomeScreenController::IsHomeScreenAvailable() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

void HomeScreenController::Show() {
  DCHECK(IsHomeScreenAvailable());

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  delegate_->ShowHomeScreen();
  UpdateVisibility();

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (window)
    Shelf::ForWindow(window)->MaybeUpdateShelfBackground();
}

void HomeScreenController::SetDelegate(HomeScreenDelegate* delegate) {
  delegate_ = delegate;
}

void HomeScreenController::OnWindowDragStarted() {
  in_window_dragging_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWindowDragEnded() {
  in_window_dragging_ = false;
  UpdateVisibility();
}

void HomeScreenController::OnWallpaperPreviewStarted() {
  in_wallpaper_preview_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWallpaperPreviewEnded() {
  in_wallpaper_preview_ = false;
  UpdateVisibility();
}

void HomeScreenController::UpdateVisibility() {
  if (!IsHomeScreenAvailable())
    return;

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (!window)
    return;

  const bool in_overview = Shell::Get()->overview_controller()->IsSelecting();
  if (in_overview || in_wallpaper_preview_ || in_window_dragging_)
    window->Hide();
  else
    window->Show();
}

}  // namespace ash
