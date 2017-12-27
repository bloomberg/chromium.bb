// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/test_wallpaper_delegate.h"

namespace ash {

TestWallpaperDelegate::TestWallpaperDelegate() {}

TestWallpaperDelegate::~TestWallpaperDelegate() = default;

bool TestWallpaperDelegate::CanOpenSetWallpaperPage() {
  return true;
}

}  // namespace ash
