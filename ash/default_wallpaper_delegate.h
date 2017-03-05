// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEFAULT_WALLPAPER_DELEGATE_H_
#define ASH_DEFAULT_WALLPAPER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/common/wallpaper/wallpaper_delegate.h"
#include "base/macros.h"

namespace ash {

class ASH_EXPORT DefaultWallpaperDelegate : public WallpaperDelegate {
 public:
  DefaultWallpaperDelegate() {}
  ~DefaultWallpaperDelegate() override {}

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

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultWallpaperDelegate);
};

}  // namespace ash

#endif  // ASH_DEFAULT_WALLPAPER_DELEGATE_H_
