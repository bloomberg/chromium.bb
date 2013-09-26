// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_user_wallpaper_delegate.h"

namespace ash {
namespace test {

void TestUserWallpaperDelegate::UpdateWallpaper() {
  DefaultUserWallpaperDelegate::UpdateWallpaper();
  update_wallpaper_count_ ++;
}

int TestUserWallpaperDelegate::GetUpdateWallpaperCountAndReset() {
  int count = update_wallpaper_count_;
  update_wallpaper_count_ = 0;
  return count;
}

}  // namespace test
}  // namespace ash
