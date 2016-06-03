// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSUI_USER_WALLPAPER_DELEGATE_MUS_H_
#define ASH_SYSUI_USER_WALLPAPER_DELEGATE_MUS_H_

#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/sysui/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"

namespace ash {
namespace sysui {

class UserWallpaperDelegateMus : public UserWallpaperDelegate,
                                 public mojom::WallpaperController {
 public:
  UserWallpaperDelegateMus();
  ~UserWallpaperDelegateMus() override;

 private:
  // UserWallpaperDelegate overrides:
  int GetAnimationType() override;
  int GetAnimationDurationOverride() override;
  void SetAnimationDurationOverride(int animation_duration_in_ms) override;
  bool ShouldShowInitialAnimation() override;
  void UpdateWallpaper(bool clear_cache) override;
  void InitializeWallpaper() override;
  void OpenSetWallpaperPage() override;
  bool CanOpenSetWallpaperPage() override;
  void OnWallpaperAnimationFinished() override;
  void OnWallpaperBootAnimationFinished() override;

  // mojom::WallpaperController overrides:
  void SetWallpaper(const SkBitmap& wallpaper,
                    mojom::WallpaperLayout layout) override;

  DISALLOW_COPY_AND_ASSIGN(UserWallpaperDelegateMus);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_SYSUI_USER_WALLPAPER_DELEGATE_MUS_H_
