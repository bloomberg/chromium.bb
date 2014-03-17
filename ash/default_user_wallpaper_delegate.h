// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DEFAULT_USER_WALLPAPER_DELEGATE_H_
#define ASH_DEFAULT_USER_WALLPAPER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace ash {

class ASH_EXPORT DefaultUserWallpaperDelegate : public UserWallpaperDelegate {
 public:
  DefaultUserWallpaperDelegate() {}
  virtual ~DefaultUserWallpaperDelegate() {}

  // UserWallpaperDelegate overrides:
  virtual int GetAnimationType() OVERRIDE;
  virtual int GetAnimationDurationOverride() OVERRIDE;
  virtual void SetAnimationDurationOverride(
      int animation_duration_in_ms) OVERRIDE;
  virtual bool ShouldShowInitialAnimation() OVERRIDE;
  virtual void UpdateWallpaper(bool clear_cache) OVERRIDE;
  virtual void InitializeWallpaper() OVERRIDE;
  virtual void OpenSetWallpaperPage() OVERRIDE;
  virtual bool CanOpenSetWallpaperPage() OVERRIDE;
  virtual void OnWallpaperAnimationFinished() OVERRIDE;
  virtual void OnWallpaperBootAnimationFinished() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultUserWallpaperDelegate);
};

}  // namespace ash

#endif  // ASH_DEFAULT_USER_WALLPAPER_DELEGATE_H_
