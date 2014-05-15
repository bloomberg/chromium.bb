// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"

typedef unsigned int SkColor;

namespace aura {
class Window;
}

namespace ash {

enum WallpaperLayout {
  // Center the wallpaper on the desktop without scaling it. The wallpaper
  // may be cropped.
  WALLPAPER_LAYOUT_CENTER,
  // Scale the wallpaper (while preserving its aspect ratio) to cover the
  // desktop; the wallpaper may be cropped.
  WALLPAPER_LAYOUT_CENTER_CROPPED,
  // Scale the wallpaper (without preserving its aspect ratio) to match the
  // desktop's size.
  WALLPAPER_LAYOUT_STRETCH,
  // Tile the wallpaper over the background without scaling it.
  WALLPAPER_LAYOUT_TILE
};

const SkColor kLoginWallpaperColor = 0xFEFEFE;

class DesktopBackgroundControllerObserver;
class WallpaperResizer;

// Updates background layer if necessary.
class ASH_EXPORT DesktopBackgroundController
    : public DisplayController::Observer,
      public ShellObserver {
 public:
  class TestAPI;

  enum BackgroundMode {
    BACKGROUND_NONE,
    BACKGROUND_IMAGE,
  };

  DesktopBackgroundController();
  virtual ~DesktopBackgroundController();

  BackgroundMode desktop_background_mode() const {
    return desktop_background_mode_;
  }

  // Add/Remove observers.
  void AddObserver(DesktopBackgroundControllerObserver* observer);
  void RemoveObserver(DesktopBackgroundControllerObserver* observer);

  // Provides current image on the background, or empty gfx::ImageSkia if there
  // is no image, e.g. background is none.
  gfx::ImageSkia GetWallpaper() const;

  WallpaperLayout GetWallpaperLayout() const;

  // Sets wallpaper. This is mostly called by WallpaperManager to set
  // the default or user selected custom wallpaper.
  // Returns true if new image was actually set. And false when duplicate set
  // request detected.
  bool SetWallpaperImage(const gfx::ImageSkia& image, WallpaperLayout layout);

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created
  // asynchronously. If loading a real wallpaper, there are cases that these
  // tests crash because the required widget is not ready. This function
  // synchronously creates an empty widget for those tests to prevent
  // crashes. An example test is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Move all desktop widgets to locked container.
  // Returns true if the desktop moved.
  bool MoveDesktopToLockedContainer();

  // Move all desktop widgets to unlocked container.
  // Returns true if the desktop moved.
  bool MoveDesktopToUnlockedContainer();

  // DisplayController::Observer:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

  // ShellObserver:
  virtual void OnRootWindowAdded(aura::Window* root_window) OVERRIDE;

  // Returns the maximum size of all displays combined in native
  // resolutions.  Note that this isn't the bounds of the display who
  // has maximum resolutions. Instead, this returns the size of the
  // maximum width of all displays, and the maximum height of all displays.
  static gfx::Size GetMaxDisplaySizeInNative();

  // Returns true if the specified wallpaper is already stored
  // in |current_wallpaper_|.
  // If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                WallpaperLayout layout) const;

 private:
  friend class DesktopBackgroundControllerTest;
  //  friend class chromeos::WallpaperManagerBrowserTestDefaultWallpaper;
  FRIEND_TEST_ALL_PREFIXES(DesktopBackgroundControllerTest, GetMaxDisplaySize);

  // Creates view for all root windows, or notifies them to repaint if they
  // already exist.
  void SetDesktopBackgroundImageMode();

  // Creates and adds component for current mode (either Widget or Layer) to
  // |root_window|.
  void InstallDesktopController(aura::Window* root_window);

  // Creates and adds component for current mode (either Widget or Layer) to
  // all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves all desktop components from one container to other across all root
  // windows. Returns true if a desktop moved.
  bool ReparentBackgroundWidgets(int src_container, int dst_container);

  // Returns id for background container for unlocked and locked states.
  int GetBackgroundContainerId(bool locked);

  // Send notification that background animation finished.
  void NotifyAnimationFinished();

  // Reload the wallpaper.
  void UpdateWallpaper();

  void set_wallpaper_reload_delay_for_test(bool value) {
    wallpaper_reload_delay_ = value;
  }

  // Can change at runtime.
  bool locked_;

  BackgroundMode desktop_background_mode_;

  SkColor background_color_;

  ObserverList<DesktopBackgroundControllerObserver> observers_;

  // The current wallpaper.
  scoped_ptr<WallpaperResizer> current_wallpaper_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer<DesktopBackgroundController> timer_;

  int wallpaper_reload_delay_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
