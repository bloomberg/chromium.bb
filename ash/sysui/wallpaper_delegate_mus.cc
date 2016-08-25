// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/sysui/wallpaper_delegate_mus.h"

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/shell.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "services/shell/public/cpp/connector.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/wm/core/window_animations.h"

namespace {

wallpaper::WallpaperLayout WallpaperLayoutFromMojo(
    ash::mojom::WallpaperLayout layout) {
  switch (layout) {
    case ash::mojom::WallpaperLayout::CENTER:
      return wallpaper::WALLPAPER_LAYOUT_CENTER;
    case ash::mojom::WallpaperLayout::CENTER_CROPPED:
      return wallpaper::WALLPAPER_LAYOUT_CENTER_CROPPED;
    case ash::mojom::WallpaperLayout::STRETCH:
      return wallpaper::WALLPAPER_LAYOUT_STRETCH;
    case ash::mojom::WallpaperLayout::TILE:
      return wallpaper::WALLPAPER_LAYOUT_TILE;
  }
  NOTREACHED();
  return wallpaper::WALLPAPER_LAYOUT_CENTER;
}

}  // namespace

namespace ash {
namespace sysui {

WallpaperDelegateMus::WallpaperDelegateMus() {}

WallpaperDelegateMus::~WallpaperDelegateMus() {}

int WallpaperDelegateMus::GetAnimationType() {
  return ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE;
}

int WallpaperDelegateMus::GetAnimationDurationOverride() {
  return 0;
}

void WallpaperDelegateMus::SetAnimationDurationOverride(
    int animation_duration_in_ms) {
  NOTIMPLEMENTED();
}

bool WallpaperDelegateMus::ShouldShowInitialAnimation() {
  return false;
}

void WallpaperDelegateMus::UpdateWallpaper(bool clear_cache) {
  NOTIMPLEMENTED();
}

void WallpaperDelegateMus::InitializeWallpaper() {
  // No action required; ChromeBrowserMainPartsChromeos inits WallpaperManager.
}

void WallpaperDelegateMus::OpenSetWallpaperPage() {
  mojom::WallpaperManagerPtr wallpaper_manager;
  auto* connector = views::WindowManagerConnection::Get()->connector();
  connector->ConnectToInterface("exe:chrome", &wallpaper_manager);
  wallpaper_manager->Open();
}

bool WallpaperDelegateMus::CanOpenSetWallpaperPage() {
  // TODO(msw): Restrict this during login, etc.
  return true;
}

void WallpaperDelegateMus::OnWallpaperAnimationFinished() {}

void WallpaperDelegateMus::OnWallpaperBootAnimationFinished() {}

void WallpaperDelegateMus::SetWallpaper(const SkBitmap& wallpaper,
                                        mojom::WallpaperLayout layout) {
  if (wallpaper.isNull())
    return;
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(wallpaper);
  Shell::GetInstance()->desktop_background_controller()->SetWallpaperImage(
      image, WallpaperLayoutFromMojo(layout));
}

}  // namespace sysui
}  // namespace ash
