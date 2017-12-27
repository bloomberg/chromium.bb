// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_DELEGATE_H_
#define ASH_WALLPAPER_WALLPAPER_DELEGATE_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT WallpaperDelegate {
 public:
  virtual ~WallpaperDelegate() {}

  // Returns the type of window animation that should be used when showing the
  // wallpaper.
  virtual int GetAnimationType() = 0;

  // Returns the wallpaper animation duration in ms. A value of 0 indicates
  // that the default should be used.
  virtual int GetAnimationDurationOverride() = 0;

  // Sets wallpaper animation duration in ms. Pass 0 to use the default.
  virtual void SetAnimationDurationOverride(int animation_duration_in_ms) = 0;

  // Should the slower initial animation be shown (as opposed to the faster
  // animation that's used e.g. when switching from one user's wallpaper to
  // another's on the login screen)?
  virtual bool ShouldShowInitialAnimation() = 0;

  // Initialize wallpaper.
  virtual void InitializeWallpaper() = 0;

  // Returns true if user can open set wallpaper page.
  virtual bool CanOpenSetWallpaperPage() = 0;

  // Notifies delegate that wallpaper animation has finished.
  virtual void OnWallpaperAnimationFinished() = 0;

  // Notifies delegate that wallpaper boot animation has finished.
  virtual void OnWallpaperBootAnimationFinished() = 0;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_DELEGATE_H_
