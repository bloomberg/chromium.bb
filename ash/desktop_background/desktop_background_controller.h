// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"

class SkBitmap;

namespace ash {

class UserWallpaperDelegate {
 public:
  virtual ~UserWallpaperDelegate() {}

  // Gets the index of user selected wallpaper.
  virtual const int GetUserWallpaperIndex() = 0;

  // Open the set wallpaper page in the browser.
  virtual void OpenSetWallpaperPage() = 0;

  // Returns true if user can open set wallpaper page. Only guest user returns
  // false currently.
  virtual bool CanOpenSetWallpaperPage() = 0;
};

// A class to listen for login and desktop background change events and set the
// corresponding default wallpaper in Aura shell.
class ASH_EXPORT DesktopBackgroundController {
 public:
  enum BackgroundMode {
    BACKGROUND_IMAGE,
    BACKGROUND_SOLID_COLOR
  };

  DesktopBackgroundController();
  virtual ~DesktopBackgroundController();

  // Get the desktop background mode.
  BackgroundMode desktop_background_mode() const {
    return desktop_background_mode_;
  }

  // Load default wallpaper at |index| asynchronously and set to current
  // wallpaper after loaded.
  void SetDefaultWallpaper(int index);

  // Cancel the current wallpaper loading operation.
  void CancelPendingWallpaperOperation();

  // Load logged in user wallpaper asynchronously and set to current wallpaper
  // after loaded.
  void SetLoggedInUserWallpaper();

  // Sets the desktop background to solid color mode and create a solid color
  // layout.
  void SetDesktopBackgroundSolidColorMode();

 private:
  // An operation to asynchronously load wallpaper.
  class WallpaperOperation;

  // Sets the desktop background to image mode and create a new background
  // widget with user selected wallpaper or default wallpaper. Delete the old
  // widget if any.
  void SetDesktopBackgroundImageMode(scoped_refptr<WallpaperOperation> wo);

  // Default wallpapper loaded, set the background mode to image mode.
  void OnWallpaperLoadCompleted(scoped_refptr<WallpaperOperation> wo);

  // Create an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created asynchronously
  // . If loading a real wallpaper, there are cases that these tests crash
  // because the required widget is not ready. This function synchronously
  // creates an empty widget for those tests to prevent crashes. An example test
  // is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Can change at runtime.
  BackgroundMode desktop_background_mode_;

  // The previous successfully loaded wallpaper.
  int previous_index_;

  scoped_refptr<WallpaperOperation> wallpaper_op_;

  base::WeakPtrFactory<DesktopBackgroundController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundController);
};

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_CONTROLLER_H_
