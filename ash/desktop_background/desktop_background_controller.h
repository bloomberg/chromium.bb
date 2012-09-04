// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "ash/wm/window_animations.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/image/image_skia.h"

typedef unsigned int SkColor;

namespace aura {
class RootWindow;
}

namespace ash {

// The width and height of small/large resolution wallpaper. When screen size is
// smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|, the
// small resolution wallpaper should be used. Otherwise, uses the large
// resolution wallpaper.
ASH_EXPORT extern const int kSmallWallpaperMaxWidth;
ASH_EXPORT extern const int kSmallWallpaperMaxHeight;
ASH_EXPORT extern const int kLargeWallpaperMaxWidth;
ASH_EXPORT extern const int kLargeWallpaperMaxHeight;

class UserWallpaperDelegate {
 public:
  virtual ~UserWallpaperDelegate() {}

  // Returns type of window animation that should be used when showin wallpaper.
  virtual ash::WindowVisibilityAnimationType GetAnimationType() = 0;

  // Initialize wallpaper.
  virtual void InitializeWallpaper() = 0;

  // Opens the set wallpaper page in the browser.
  virtual void OpenSetWallpaperPage() = 0;

  // Returns true if user can open set wallpaper page. Only guest user returns
  // false currently.
  virtual bool CanOpenSetWallpaperPage() = 0;

  // Notifies delegate that wallpaper animation has finished.
  virtual void OnWallpaperAnimationFinished() = 0;

  // Notifies delegate that wallpaper boot animation has finished.
  virtual void OnWallpaperBootAnimationFinished() = 0;
};

// Loads selected desktop wallpaper from file system asynchronously and updates
// background layer if loaded successfully.
class ASH_EXPORT DesktopBackgroundController : public aura::WindowObserver {
 public:
  enum BackgroundMode {
    BACKGROUND_IMAGE,
    BACKGROUND_SOLID_COLOR
  };

  DesktopBackgroundController();
  virtual ~DesktopBackgroundController();

  // Gets the desktop background mode.
  BackgroundMode desktop_background_mode() const {
    return desktop_background_mode_;
  }

  gfx::ImageSkia GetWallpaper() const;

  WallpaperLayout GetWallpaperLayout() const;

  // Provides current image on the background, or empty gfx::ImageSkia if there
  // is no image, e.g. background is solid color.
  gfx::ImageSkia GetCurrentWallpaperImage();

  // Initialize root window's background.
  void OnRootWindowAdded(aura::RootWindow* root_window);

  // Loads default wallpaper at |index| asynchronously but does not set the
  // loaded image to current wallpaper. Resource bundle will cache the loaded
  // image.
  void CacheDefaultWallpaper(int index);

  // Loads default wallpaper at |index| asynchronously and sets to current
  // wallpaper after loaded. When |force_reload| is true, reload wallpaper
  // for all root windows even if |index| is the same as current wallpaper. It
  // must be true when a different resolution of current wallpaper is needed.
  void SetDefaultWallpaper(int index, bool force_reload);

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
  void MoveDesktopToLockedContainer();

  // Move all desktop widgets to unlocked container.
  void MoveDesktopToUnlockedContainer();

  // Drop references to background view for |root_window|, because the view
  // was deleted.
  void CleanupView(aura::RootWindow* root_window);

  // WindowObserver implementation.
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

 private:
  // An operation to asynchronously loads wallpaper.
  class WallpaperOperation;

  struct WallpaperData;

  // Creates view for all root windows, or notifies them to repaint if they
  // already exist.
  void SetDesktopBackgroundImageMode();

  // Creates a new background widget and sets the background mode to image mode.
  // Called after wallpaper loaded successfully.
  void OnWallpaperLoadCompleted(scoped_refptr<WallpaperOperation> wo);

  // Adds layer with solid |color| to container |container_id| in |root_window|.
  ui::Layer* SetColorLayerForContainer(SkColor color,
                                       aura::RootWindow* root_window,
                                       int container_id);

  // Creates and adds component for current mode (either Widget or Layer) to
  // |root_window|.
  void InstallComponent(aura::RootWindow* root_window);

  // Creates and adds component for current mode (either Widget or Layer) to
  // all root windows.
  void InstallComponentForAllWindows();

  // Moves all descktop components from one container to other across all root
  // windows.
  void ReparentBackgroundWidgets(int src_container, int dst_container);

  // Returns id for background container for unlocked and locked states.
  int GetBackgroundContainerId(bool locked);

  // Send notification that background animation finished.
  void NotifyAnimationFinished();

  // Can change at runtime.
  bool locked_;

  BackgroundMode desktop_background_mode_;

  SkColor background_color_;

  // The current wallpaper.
  scoped_ptr<WallpaperData> current_wallpaper_;

  scoped_refptr<WallpaperOperation> wallpaper_op_;

  base::WeakPtrFactory<DesktopBackgroundController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
