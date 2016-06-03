// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/user_wallpaper_delegate_mus.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/wm/core/window_animations.h"

namespace {

wallpaper::WallpaperLayout WallpaperLayoutFromMojo(
    ash::sysui::mojom::WallpaperLayout layout) {
  switch (layout) {
    case ash::sysui::mojom::WallpaperLayout::CENTER:
      return wallpaper::WALLPAPER_LAYOUT_CENTER;
    case ash::sysui::mojom::WallpaperLayout::CENTER_CROPPED:
      return wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
    case ash::sysui::mojom::WallpaperLayout::STRETCH:
      return wallpaper::WALLPAPER_LAYOUT_STRETCH;
    case ash::sysui::mojom::WallpaperLayout::TILE:
      return wallpaper::WALLPAPER_LAYOUT_TILE;
  }
  NOTREACHED();
  return wallpaper::WALLPAPER_LAYOUT_CENTER;
}

}  // namespace

namespace ash {
namespace sysui {

UserWallpaperDelegateMus::UserWallpaperDelegateMus() {}

UserWallpaperDelegateMus::~UserWallpaperDelegateMus() {}

int UserWallpaperDelegateMus::GetAnimationType() {
  return ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

int UserWallpaperDelegateMus::GetAnimationDurationOverride() {
  return 0;
}

void UserWallpaperDelegateMus::SetAnimationDurationOverride(
    int animation_duration_in_ms) {
  NOTIMPLEMENTED();
}

bool UserWallpaperDelegateMus::ShouldShowInitialAnimation() {
  return false;
}

void UserWallpaperDelegateMus::UpdateWallpaper(bool clear_cache) {
  NOTIMPLEMENTED();
}

void UserWallpaperDelegateMus::InitializeWallpaper() {
  // No action required; ChromeBrowserMainPartsChromeos inits WallpaperManager.
}

void UserWallpaperDelegateMus::OpenSetWallpaperPage() {
  mojom::WallpaperManagerPtr wallpaper_manager;
  auto* connector = views::WindowManagerConnection::Get()->connector();
  connector->ConnectToInterface("exe:chrome", &wallpaper_manager);
  wallpaper_manager->Open();
}

bool UserWallpaperDelegateMus::CanOpenSetWallpaperPage() {
  // TODO(msw): Restrict this during login, etc.
  return true;
}

void UserWallpaperDelegateMus::OnWallpaperAnimationFinished() {}

void UserWallpaperDelegateMus::OnWallpaperBootAnimationFinished() {}

void UserWallpaperDelegateMus::SetWallpaper(const SkBitmap& wallpaper,
                                            mojom::WallpaperLayout layout) {
  if (wallpaper.isNull())
    return;
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(wallpaper);
  Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      image, WallpaperLayoutFromMojo(layout));
}

}  // namespace sysui
}  // namespace ash
