// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"

typedef unsigned int SkColor;

namespace aura {
class RootWindow;
}

namespace ash {
namespace internal {
class DesktopBackgroundControllerTest;
}  // namespace internal

enum WallpaperLayout {
  WALLPAPER_LAYOUT_CENTER,
  WALLPAPER_LAYOUT_CENTER_CROPPED,
  WALLPAPER_LAYOUT_STRETCH,
  WALLPAPER_LAYOUT_TILE,
};

enum WallpaperResolution {
  WALLPAPER_RESOLUTION_LARGE,
  WALLPAPER_RESOLUTION_SMALL
};

const SkColor kLoginWallpaperColor = 0xFEFEFE;

// Encapsulates wallpaper infomation needed by desktop background controller.
struct ASH_EXPORT WallpaperInfo {
  int idr;
  WallpaperLayout layout;
};

ASH_EXPORT extern const WallpaperInfo kDefaultLargeWallpaper;
ASH_EXPORT extern const WallpaperInfo kDefaultSmallWallpaper;
ASH_EXPORT extern const WallpaperInfo kGuestLargeWallpaper;
ASH_EXPORT extern const WallpaperInfo kGuestSmallWallpaper;

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
class ASH_EXPORT DesktopBackgroundController : public aura::WindowObserver {
 public:
  enum BackgroundMode {
    BACKGROUND_NONE,
    BACKGROUND_IMAGE,
    BACKGROUND_SOLID_COLOR
  };

  DesktopBackgroundController();
  virtual ~DesktopBackgroundController();

  // Gets the desktop background mode.
  BackgroundMode desktop_background_mode() const {
    return desktop_background_mode_;
  }

  // Add/Remove observers.
  void AddObserver(DesktopBackgroundControllerObserver* observer);
  void RemoveObserver(DesktopBackgroundControllerObserver* observer);

  gfx::ImageSkia GetWallpaper() const;

  WallpaperLayout GetWallpaperLayout() const;

  // Provides current image on the background, or empty gfx::ImageSkia if there
  // is no image, e.g. background is solid color.
  gfx::ImageSkia GetCurrentWallpaperImage();

  // Gets the IDR of current wallpaper. Returns -1 if current wallpaper is not
  // a builtin wallpaper.
  int GetWallpaperIDR() const;

  // Initialize root window's background.
  void OnRootWindowAdded(aura::RootWindow* root_window);

  // Loads builtin wallpaper asynchronously and sets to current wallpaper after
  // loaded.
  void SetDefaultWallpaper(const WallpaperInfo& info);

  // Sets the user selected custom wallpaper. Called when user selected a file
  // from file system or changed the layout of wallpaper.
  void SetCustomWallpaper(const gfx::ImageSkia& wallpaper,
                          WallpaperLayout layout);

  // Cancels the current wallpaper loading operation.
  void CancelPendingWallpaperOperation();

  // Sets the desktop background to solid color mode and creates a solid
  // |color| layout.
  void SetDesktopBackgroundSolidColorMode(SkColor color);

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

  // WindowObserver implementation.
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  friend class internal::DesktopBackgroundControllerTest;

  // An operation to asynchronously loads wallpaper.
  class WallpaperLoader;

  // Creates view for all root windows, or notifies them to repaint if they
  // already exist.
  void SetDesktopBackgroundImageMode();

  // Creates a new background widget and sets the background mode to image mode.
  // Called after wallpaper loaded successfully.
  void OnWallpaperLoadCompleted(scoped_refptr<WallpaperLoader> wl);

  // Adds layer with solid |color| to container |container_id| in |root_window|.
  ui::Layer* SetColorLayerForContainer(SkColor color,
                                       aura::RootWindow* root_window,
                                       int container_id);

  // Creates and adds component for current mode (either Widget or Layer) to
  // |root_window|.
  void InstallDesktopController(aura::RootWindow* root_window);

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

  // Can change at runtime.
  bool locked_;

  BackgroundMode desktop_background_mode_;

  SkColor background_color_;

  ObserverList<DesktopBackgroundControllerObserver> observers_;

  // The current wallpaper.
  scoped_ptr<WallpaperResizer> current_wallpaper_;

  scoped_refptr<WallpaperLoader> wallpaper_loader_;

  base::WeakPtrFactory<DesktopBackgroundController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
