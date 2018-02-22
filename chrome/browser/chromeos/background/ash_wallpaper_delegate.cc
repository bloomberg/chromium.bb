// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/background/ash_wallpaper_delegate.h"

#include "ash/wallpaper/wallpaper_delegate.h"
#include "base/macros.h"

namespace chromeos {

namespace {

class WallpaperDelegate : public ash::WallpaperDelegate {
 public:
  WallpaperDelegate() = default;

  ~WallpaperDelegate() override = default;

  int GetAnimationDurationOverride() override {
    return animation_duration_override_in_ms_;
  }

  void SetAnimationDurationOverride(int animation_duration_in_ms) override {
    animation_duration_override_in_ms_ = animation_duration_in_ms;
  }

 private:
  // The animation duration to show a new wallpaper if an animation is required.
  int animation_duration_override_in_ms_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WallpaperDelegate);
};

}  // namespace

ash::WallpaperDelegate* CreateWallpaperDelegate() {
  return new chromeos::WallpaperDelegate();
}

}  // namespace chromeos
