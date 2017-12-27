// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/default_wallpaper_delegate.h"

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

int DefaultWallpaperDelegate::GetAnimationType() {
  return ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

int DefaultWallpaperDelegate::GetAnimationDurationOverride() {
  return 0;
}

void DefaultWallpaperDelegate::SetAnimationDurationOverride(
    int animation_duration_in_ms) {}

bool DefaultWallpaperDelegate::ShouldShowInitialAnimation() {
  return false;
}

void DefaultWallpaperDelegate::InitializeWallpaper() {
  Shell::Get()->wallpaper_controller()->CreateEmptyWallpaper();
}

bool DefaultWallpaperDelegate::CanOpenSetWallpaperPage() {
  return false;
}

void DefaultWallpaperDelegate::OnWallpaperAnimationFinished() {}

void DefaultWallpaperDelegate::OnWallpaperBootAnimationFinished() {}

}  // namespace ash
