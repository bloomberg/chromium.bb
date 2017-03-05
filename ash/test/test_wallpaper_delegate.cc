// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_wallpaper_delegate.h"

#include "ash/common/wallpaper/wallpaper_controller.h"
#include "ash/common/wm_shell.h"

namespace ash {
namespace test {

TestWallpaperDelegate::TestWallpaperDelegate() : update_wallpaper_count_(0) {}

TestWallpaperDelegate::~TestWallpaperDelegate() {}

void TestWallpaperDelegate::UpdateWallpaper(bool clear_cache) {
  DefaultWallpaperDelegate::UpdateWallpaper(clear_cache);
  if (!custom_wallpaper_.isNull()) {
    WmShell::Get()->wallpaper_controller()->SetWallpaperImage(
        custom_wallpaper_, wallpaper::WALLPAPER_LAYOUT_STRETCH);
  }
  update_wallpaper_count_++;
}

int TestWallpaperDelegate::GetUpdateWallpaperCountAndReset() {
  int count = update_wallpaper_count_;
  update_wallpaper_count_ = 0;
  return count;
}

}  // namespace test
}  // namespace ash
