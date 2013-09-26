// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_USER_WALLPAPER_DELEGATE_H_
#define ASH_TEST_TEST_USER_WALLPAPER_DELEGATE_H_

#include "ash/default_user_wallpaper_delegate.h"

namespace ash {
namespace test {

class TestUserWallpaperDelegate : public DefaultUserWallpaperDelegate {
 public:
  TestUserWallpaperDelegate() : update_wallpaper_count_(0) {}
  virtual ~TestUserWallpaperDelegate() {}

  // DefaultUserWallpaperDelegate overrides:
  virtual void UpdateWallpaper() OVERRIDE;

  int GetUpdateWallpaperCountAndReset();

 private:
  int update_wallpaper_count_;

  DISALLOW_COPY_AND_ASSIGN(TestUserWallpaperDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_USER_WALLPAPER_DELEGATE_H_
