// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
#define ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wallpaper/wallpaper_controller_observer.h"
#include "base/macros.h"
#include "base/scoped_observer.h"

namespace ash {

class HomeLauncherGestureHandler;
class HomeScreenDelegate;
class WallpaperController;

// HomeScreenController handles the home launcher (e.g., tablet-mode app list)
// and owns the HomeLauncherGestureHandler that transitions the launcher window
// and other windows when the launcher is shown, hidden or animated.
class ASH_EXPORT HomeScreenController : public WallpaperControllerObserver {
 public:
  HomeScreenController();
  ~HomeScreenController() override;

  // Returns true if the home screen can be shown (generally corresponds to the
  // device being in tablet mode).
  bool IsHomeScreenAvailable();

  // Shows the home screen.
  void Show();

  // Sets the delegate for home screen animations.
  void SetDelegate(HomeScreenDelegate* delegate);

  // Called when a window starts/ends dragging. If the home screen is shown, we
  // should hide it during dragging a window and reshow it when the drag ends.
  void OnWindowDragStarted();
  void OnWindowDragEnded();

  HomeLauncherGestureHandler* home_launcher_gesture_handler() {
    return home_launcher_gesture_handler_.get();
  }

  HomeScreenDelegate* delegate() { return delegate_; }

 private:
  // WallpaperControllerObserver:
  void OnWallpaperPreviewStarted() override;
  void OnWallpaperPreviewEnded() override;

  // Updates the visibility of the home screen based on e.g. if the device is
  // in overview mode.
  void UpdateVisibility();

  // Whether the wallpaper is being previewed. The home screen should be hidden
  // during wallpaper preview.
  bool in_wallpaper_preview_ = false;

  // Whether we're currently in a window dragging process.
  bool in_window_dragging_ = false;

  // Not owned.
  HomeScreenDelegate* delegate_ = nullptr;

  // Owned pointer to the object which handles gestures related to the home
  // launcher.
  std::unique_ptr<HomeLauncherGestureHandler> home_launcher_gesture_handler_;

  ScopedObserver<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(HomeScreenController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_HOME_SCREEN_CONTROLLER_H_
