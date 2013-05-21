// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/desktop_background/user_wallpaper_delegate.h"

#include "ash/desktop_background/desktop_background_controller.h"
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

  virtual int GetAnimationType() OVERRIDE {
    return ShouldShowInitialAnimation() ?
        ash::WINDOW_VISIBILITY_ANIMATION_TYPE_BRIGHTNESS_GRAYSCALE :
        static_cast<int>(views::corewm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  }

  virtual bool ShouldShowInitialAnimation() OVERRIDE {
    return true;
  }

  virtual void UpdateWallpaper() OVERRIDE {
  }

  virtual void InitializeWallpaper() OVERRIDE {
    SkBitmap bitmap;
    bitmap.setConfig(SkBitmap::kARGB_8888_Config, 16, 16);
    bitmap.allocPixels();
    bitmap.eraseARGB(255, kBackgroundRed, kBackgroundGreen, kBackgroundBlue);
    gfx::ImageSkia wallpaper =
        gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    ash::Shell::GetInstance()->desktop_background_controller()->
        SetCustomWallpaper(wallpaper, ash::WALLPAPER_LAYOUT_TILE);
  }

  virtual void OpenSetWallpaperPage() OVERRIDE {
  }

  virtual bool CanOpenSetWallpaperPage() OVERRIDE {
    return false;
  }

  virtual void OnWallpaperAnimationFinished() OVERRIDE {
  }

  virtual void OnWallpaperBootAnimationFinished() OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserWallpaperDelegate);
};

}  // namespace

ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() {
  return new UserWallpaperDelegate();
}
