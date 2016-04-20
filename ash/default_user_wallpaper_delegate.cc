// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/default_user_wallpaper_delegate.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

int DefaultUserWallpaperDelegate::GetAnimationType() {
  return ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

int DefaultUserWallpaperDelegate::GetAnimationDurationOverride() {
  return 0;
}

void DefaultUserWallpaperDelegate::SetAnimationDurationOverride(
    int animation_duration_in_ms) {
}

bool DefaultUserWallpaperDelegate::ShouldShowInitialAnimation() {
  return false;
}

void DefaultUserWallpaperDelegate::UpdateWallpaper(bool clear_cache) {
}

void DefaultUserWallpaperDelegate::InitializeWallpaper() {
  ash::Shell::GetInstance()->desktop_background_controller()->
      CreateEmptyWallpaper();
}

void DefaultUserWallpaperDelegate::OpenSetWallpaperPage() {
}

bool DefaultUserWallpaperDelegate::CanOpenSetWallpaperPage() {
  return false;
}

void DefaultUserWallpaperDelegate::OnWallpaperAnimationFinished() {
}

void DefaultUserWallpaperDelegate::OnWallpaperBootAnimationFinished() {
}

}  // namespace ash
