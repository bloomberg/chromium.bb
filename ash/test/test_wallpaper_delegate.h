// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_WALLPAPER_DELEGATE_H_
#define ASH_TEST_TEST_WALLPAPER_DELEGATE_H_

#include "ash/default_wallpaper_delegate.h"

#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace test {

class TestWallpaperDelegate : public DefaultWallpaperDelegate {
 public:
  TestWallpaperDelegate();
  ~TestWallpaperDelegate() override;

  void set_custom_wallpaper(const gfx::ImageSkia& wallpaper) {
    custom_wallpaper_ = wallpaper;
  }

  // DefaultWallpaperDelegate overrides:
  void UpdateWallpaper(bool clear_cache) override;

  // Returns and clears |update_wallpaper_count_|.
  int GetUpdateWallpaperCountAndReset();

 private:
  // Number of times that UpdateWallpaper() has been called.
  int update_wallpaper_count_;

  // If non-null, used as custom wallpaper by UpdateWallpaper().
  gfx::ImageSkia custom_wallpaper_;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_WALLPAPER_DELEGATE_H_
