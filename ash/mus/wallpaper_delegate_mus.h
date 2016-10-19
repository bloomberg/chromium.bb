// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_WALLPAPER_DELEGATE_MUS_H_
#define ASH_MUS_WALLPAPER_DELEGATE_MUS_H_

#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "base/macros.h"

namespace ash {

class WallpaperDelegateMus : public WallpaperDelegate {
 public:
  WallpaperDelegateMus();
  ~WallpaperDelegateMus() override;

 private:
  // WallpaperDelegate overrides:
  int GetAnimationType() override;
  int GetAnimationDurationOverride() override;
  void SetAnimationDurationOverride(int animation_duration_in_ms) override;
  bool ShouldShowInitialAnimation() override;
  void UpdateWallpaper(bool clear_cache) override;
  void InitializeWallpaper() override;
  bool CanOpenSetWallpaperPage() override;
  void OnWallpaperAnimationFinished() override;
  void OnWallpaperBootAnimationFinished() override;

  DISALLOW_COPY_AND_ASSIGN(WallpaperDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_WALLPAPER_DELEGATE_MUS_H_
