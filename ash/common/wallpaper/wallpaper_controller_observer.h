// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_OBSERVER_H_
#define ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT WallpaperControllerObserver {
 public:
  // Invoked when the wallpaper data is changed.
  virtual void OnWallpaperDataChanged() = 0;

 protected:
  virtual ~WallpaperControllerObserver() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WALLPAPER_WALLPAPER_CONTROLLER_OBSERVER_H_
