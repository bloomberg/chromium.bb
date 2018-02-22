// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/default_wallpaper_delegate.h"

namespace ash {

int DefaultWallpaperDelegate::GetAnimationDurationOverride() {
  return 0;
}

void DefaultWallpaperDelegate::SetAnimationDurationOverride(
    int animation_duration_in_ms) {}

}  // namespace ash
