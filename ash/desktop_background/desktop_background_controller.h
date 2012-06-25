// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace aura {
class RootWindow;
}

namespace ash {

class UserWallpaperDelegate {
 public:
  virtual ~UserWallpaperDelegate() {}

  // Initialize wallpaper.
  virtual void InitializeWallpaper() = 0;

  // Opens the set wallpaper page in the browser.
  virtual void OpenSetWallpaperPage() = 0;

  // Returns true if user can open set wallpaper page. Only guest user returns
  // false currently.
  virtual bool CanOpenSetWallpaperPage() = 0;
};

// Loads selected desktop wallpaper from file system asynchronously and updates
// background layer if loaded successfully.
class ASH_EXPORT DesktopBackgroundController {
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

  // Provides current image on the background, or empty SkBitmap if there is
  // no image, e.g. background is solid color.
  SkBitmap GetCurrentWallpaperImage();

  // Initialize root window's background.
  void OnRootWindowAdded(aura::RootWindow* root_window);

  // Loads default wallpaper at |index| asynchronously and sets to current
  // wallpaper after loaded.
  void SetDefaultWallpaper(int index);

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

 private:
  // An operation to asynchronously loads wallpaper.
  class WallpaperOperation;

  struct WallpaperData;

  // Creates a new background widget using the current wallpapaer image and
  // use it as a background of the |root_window|. Deletes the old widget if any.
  void SetDesktopBackgroundImage(aura::RootWindow* root_window);

  // Update the background of all root windows using the current wallpaper image
  // in |current_wallpaper_|.
  void UpdateDesktopBackgroundImageMode();

  // Creates a new background widget and sets the background mode to image mode.
  // Called after wallpaper loaded successfully.
  void OnWallpaperLoadCompleted(scoped_refptr<WallpaperOperation> wo);

  // Can change at runtime.
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
