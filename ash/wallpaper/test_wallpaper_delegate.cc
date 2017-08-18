// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_delegate.h"

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"

namespace ash {

TestWallpaperDelegate::TestWallpaperDelegate() : update_wallpaper_count_(0) {}

TestWallpaperDelegate::~TestWallpaperDelegate() {}

void TestWallpaperDelegate::UpdateWallpaper(bool clear_cache) {
  DefaultWallpaperDelegate::UpdateWallpaper(clear_cache);
  if (!custom_wallpaper_.isNull()) {
    wallpaper::WallpaperInfo info("", wallpaper::WALLPAPER_LAYOUT_STRETCH,
                                  wallpaper::DEFAULT,
                                  base::Time::Now().LocalMidnight());
    Shell::Get()->wallpaper_controller()->SetWallpaperImage(custom_wallpaper_,
                                                            info);
  }
  update_wallpaper_count_++;
}

bool TestWallpaperDelegate::CanOpenSetWallpaperPage() {
  return true;
}

int TestWallpaperDelegate::GetUpdateWallpaperCountAndReset() {
  int count = update_wallpaper_count_;
  update_wallpaper_count_ = 0;
  return count;
}

}  // namespace ash
