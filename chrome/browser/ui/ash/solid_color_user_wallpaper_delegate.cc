// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/solid_color_user_wallpaper_delegate.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_animations.h"
#include "base/basictypes.h"
#include "ui/gfx/image/image_skia.h"

namespace {

const char kBackgroundRed   = 70;
const char kBackgroundGreen = 70;
const char kBackgroundBlue  = 78;

class UserWallpaperDelegate : public ash::UserWallpaperDelegate {
 public:
  UserWallpaperDelegate() {
  }

  virtual ~UserWallpaperDelegate() {
  }

  virtual int GetAnimationType() override {
    return ShouldShowInitialAnimation() ?
        ash::WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE :
        static_cast<int>(wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  virtual bool ShouldShowInitialAnimation() override {
    return true;
  }

  virtual int GetAnimationDurationOverride() override {
    // Return 0 to select the default.
    return 0;
  }

  virtual void SetAnimationDurationOverride(
      int animation_duration_in_ms) override {
    NOTIMPLEMENTED();
  }

  virtual void UpdateWallpaper(bool clear_cache) override {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 16);
    bitmap.eraseARGB(255, kBackgroundRed, kBackgroundGreen, kBackgroundBlue);
#if !defined(NDEBUG)
    // In debug builds we generate a simple pattern that allows visually
    // notice if transparency is broken.
    {
      SkAutoLockPixels alp(bitmap);
      *bitmap.getAddr32(0,0) = SkColorSetRGB(0, 0, 0);
    }
#endif
    gfx::ImageSkia wallpaper = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    ash::Shell::GetInstance()
        ->desktop_background_controller()
        ->SetWallpaperImage(wallpaper, ash::WALLPAPER_LAYOUT_TILE);
  }

  virtual void InitializeWallpaper() override {
    UpdateWallpaper(false);
  }

  virtual void OpenSetWallpaperPage() override {
  }

  virtual bool CanOpenSetWallpaperPage() override {
    return false;
  }

  virtual void OnWallpaperAnimationFinished() override {
  }

  virtual void OnWallpaperBootAnimationFinished() override {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserWallpaperDelegate);
};

}  // namespace

ash::UserWallpaperDelegate* CreateSolidColorUserWallpaperDelegate() {
  return new UserWallpaperDelegate();
}
