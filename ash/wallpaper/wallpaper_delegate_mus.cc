// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_delegate_mus.h"

#include "ui/wm/core/window_animations.h"

namespace ash {

WallpaperDelegateMus::WallpaperDelegateMus() = default;
WallpaperDelegateMus::~WallpaperDelegateMus() = default;

int WallpaperDelegateMus::GetAnimationType() {
  return ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

int WallpaperDelegateMus::GetAnimationDurationOverride() {
  return 0;
}

void WallpaperDelegateMus::SetAnimationDurationOverride(
    int animation_duration_in_ms) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool WallpaperDelegateMus::ShouldShowInitialAnimation() {
  return false;
}

void WallpaperDelegateMus::InitializeWallpaper() {
  // No action required; ChromeBrowserMainPartsChromeos inits WallpaperManager.
}

bool WallpaperDelegateMus::CanOpenSetWallpaperPage() {
  // TODO(msw): Restrict this during login, etc.
  return true;
}

void WallpaperDelegateMus::OnWallpaperAnimationFinished() {}

void WallpaperDelegateMus::OnWallpaperBootAnimationFinished() {}

}  // namespace ash
