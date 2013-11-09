// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
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

class CommandLine;

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
  WALLPAPER_LAYOUT_TILE,
};

enum WallpaperResolution {
  WALLPAPER_RESOLUTION_LARGE,
  WALLPAPER_RESOLUTION_SMALL
};

const SkColor kLoginWallpaperColor = 0xFEFEFE;

// The width and height of small/large resolution wallpaper. When screen size is
// smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|, the
// small resolution wallpaper should be used. Otherwise, uses the large
// resolution wallpaper.
ASH_EXPORT extern const int kSmallWallpaperMaxWidth;
ASH_EXPORT extern const int kSmallWallpaperMaxHeight;
ASH_EXPORT extern const int kLargeWallpaperMaxWidth;
ASH_EXPORT extern const int kLargeWallpaperMaxHeight;

// The width and heigh of wallpaper thumbnails.
ASH_EXPORT extern const int kWallpaperThumbnailWidth;
ASH_EXPORT extern const int kWallpaperThumbnailHeight;

class DesktopBackgroundControllerObserver;
class WallpaperResizer;

// Loads selected desktop wallpaper from file system asynchronously and updates
// background layer if loaded successfully.
class ASH_EXPORT DesktopBackgroundController
    : public DisplayController::Observer {
 public:
  enum BackgroundMode {
    BACKGROUND_NONE,
    BACKGROUND_IMAGE,
  };

  DesktopBackgroundController();
  virtual ~DesktopBackgroundController();

  BackgroundMode desktop_background_mode() const {
    return desktop_background_mode_;
  }

  void set_command_line_for_testing(CommandLine* command_line) {
    command_line_for_testing_ = command_line;
  }

  // Add/Remove observers.
  void AddObserver(DesktopBackgroundControllerObserver* observer);
  void RemoveObserver(DesktopBackgroundControllerObserver* observer);

  // Provides current image on the background, or empty gfx::ImageSkia if there
  // is no image, e.g. background is none.
  gfx::ImageSkia GetWallpaper() const;

  WallpaperLayout GetWallpaperLayout() const;

  // Initialize root window's background.
  void OnRootWindowAdded(aura::Window* root_window);

  // Loads builtin wallpaper asynchronously and sets to current wallpaper
  // after loaded. Returns true if the controller started loading the
  // wallpaper and false otherwise (i.e. the appropriate wallpaper was
  // already loading or loaded).
  bool SetDefaultWallpaper(bool is_guest);

  // Sets the user selected custom wallpaper. Called when user selected a file
  // from file system or changed the layout of wallpaper.
  void SetCustomWallpaper(const gfx::ImageSkia& image, WallpaperLayout layout);

  // Cancels the current wallpaper loading operation.
  void CancelPendingWallpaperOperation();

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created asynchronously
  // . If loading a real wallpaper, there are cases that these tests crash
  // because the required widget is not ready. This function synchronously
  // creates an empty widget for those tests to prevent crashes. An example test
  // is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Returns the appropriate wallpaper resolution for all root windows.
  WallpaperResolution GetAppropriateResolution();

  // Move all desktop widgets to locked container.
  // Returns true if the desktop moved.
  bool MoveDesktopToLockedContainer();

  // Move all desktop widgets to unlocked container.
  // Returns true if the desktop moved.
  bool MoveDesktopToUnlockedContainer();

  // Overrides DisplayController::Observer:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  friend class DesktopBackgroundControllerTest;
  FRIEND_TEST_ALL_PREFIXES(DesktopBackgroundControllerTest, GetMaxDisplaySize);

  // An operation to asynchronously loads wallpaper.
  class WallpaperLoader;

  // Returns true if the specified default wallpaper is already being
  // loaded by |wallpaper_loader_| or stored in |current_wallpaper_|.
  bool DefaultWallpaperIsAlreadyLoadingOrLoaded(
      const base::FilePath& image_file, int image_resource_id) const;

  // Returns true if the specified custom wallpaper is already stored
  // in |current_wallpaper_|.
  bool CustomWallpaperIsAlreadyLoaded(const gfx::ImageSkia& image) const;

  // Creates view for all root windows, or notifies them to repaint if they
  // already exist.
  void SetDesktopBackgroundImageMode();

  // Creates a new background widget and sets the background mode to image mode.
  // Called after a default wallpaper has been loaded successfully.
  void OnDefaultWallpaperLoadCompleted(scoped_refptr<WallpaperLoader> loader);

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

  // Returns the maximum size of all displays combined in native
  // resolutions.  Note that this isn't the bounds of the display who
  // has maximum resolutions. Instead, this returns the size of the
  // maximum width of all displays, and the maximum height of all displays.
  static gfx::Size GetMaxDisplaySizeInNative();

  // If non-NULL, used in place of the real command line.
  CommandLine* command_line_for_testing_;

  // Can change at runtime.
  bool locked_;

  BackgroundMode desktop_background_mode_;

  SkColor background_color_;

  ObserverList<DesktopBackgroundControllerObserver> observers_;

  // The current wallpaper.
  scoped_ptr<WallpaperResizer> current_wallpaper_;

  // If a default wallpaper is stored in |current_wallpaper_|, the path and
  // resource ID that were passed to WallpaperLoader when loading it.
  // Otherwise, empty and -1, respectively.
  base::FilePath current_default_wallpaper_path_;
  int current_default_wallpaper_resource_id_;

  gfx::Size current_max_display_size_;

  scoped_refptr<WallpaperLoader> wallpaper_loader_;

  base::WeakPtrFactory<DesktopBackgroundController> weak_ptr_factory_;

  base::OneShotTimer<DesktopBackgroundController> timer_;

  int wallpaper_reload_delay_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
