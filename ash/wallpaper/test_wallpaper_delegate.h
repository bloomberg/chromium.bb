// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_TEST_WALLPAPER_DELEGATE_H_
#define ASH_WALLPAPER_TEST_WALLPAPER_DELEGATE_H_

#include "ash/default_wallpaper_delegate.h"
#include "base/macros.h"

namespace ash {

class TestWallpaperDelegate : public DefaultWallpaperDelegate {
 public:
  TestWallpaperDelegate();
  ~TestWallpaperDelegate() override;

  // DefaultWallpaperDelegate overrides:
  bool CanOpenSetWallpaperPage() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWallpaperDelegate);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_TEST_WALLPAPER_DELEGATE_H_
