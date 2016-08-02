// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shell_observer.h"
#include "ash/common/wm_display_observer.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class SequencedWorkerPool;
}

namespace wallpaper {
class WallpaperResizer;
}

namespace ash {

class DesktopBackgroundControllerObserver;

// Updates the desktop background wallpaper.
class ASH_EXPORT DesktopBackgroundController : public WmDisplayObserver,
                                               public ShellObserver {
 public:
  enum BackgroundMode { BACKGROUND_NONE, BACKGROUND_IMAGE };

  explicit DesktopBackgroundController(
      base::SequencedWorkerPool* blocking_pool);
  ~DesktopBackgroundController() override;

  // Add/Remove observers.
  void AddObserver(DesktopBackgroundControllerObserver* observer);
  void RemoveObserver(DesktopBackgroundControllerObserver* observer);

  // Provides current image on the background, or empty gfx::ImageSkia if there
  // is no image, e.g. background is none.
  gfx::ImageSkia GetWallpaper() const;

  wallpaper::WallpaperLayout GetWallpaperLayout() const;

  // Sets wallpaper. This is mostly called by WallpaperManager to set
  // the default or user selected custom wallpaper.
  // Returns true if new image was actually set. And false when duplicate set
  // request detected.
  bool SetWallpaperImage(const gfx::ImageSkia& image,
                         wallpaper::WallpaperLayout layout);

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

  // WmDisplayObserver:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(WmWindow* root_window) override;

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
                                wallpaper::WallpaperLayout layout) const;

  void set_wallpaper_reload_delay_for_test(int value) {
    wallpaper_reload_delay_ = value;
  }

 private:
  // Creates a DesktopBackgroundWidgetController for |root_window|.
  void InstallDesktopController(WmWindow* root_window);

  // Creates a DesktopBackgroundWidgetController for all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves all desktop components from one container to other across all root
  // windows. Returns true if a desktop moved.
  bool ReparentBackgroundWidgets(int src_container, int dst_container);

  // Returns id for background container for unlocked and locked states.
  int GetBackgroundContainerId(bool locked);

  // Reload the wallpaper. |clear_cache| specifies whether to clear the
  // wallpaper cahce or not.
  void UpdateWallpaper(bool clear_cache);

  bool locked_;

  BackgroundMode desktop_background_mode_;

  base::ObserverList<DesktopBackgroundControllerObserver> observers_;

  std::unique_ptr<wallpaper::WallpaperResizer> current_wallpaper_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  int wallpaper_reload_delay_;

  base::SequencedWorkerPool* blocking_pool_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
