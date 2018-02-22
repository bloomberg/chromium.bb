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

  // Returns the wallpaper animation duration in ms. A value of 0 indicates
  // that the default should be used.
  virtual int GetAnimationDurationOverride() = 0;

  // Sets wallpaper animation duration in ms. Pass 0 to use the default.
  virtual void SetAnimationDurationOverride(int animation_duration_in_ms) = 0;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_DELEGATE_H_
