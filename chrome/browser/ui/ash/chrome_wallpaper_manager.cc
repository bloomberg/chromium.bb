// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_wallpaper_manager.h"

#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"

ChromeWallpaperManager::ChromeWallpaperManager() {}

ChromeWallpaperManager::~ChromeWallpaperManager() {}

void ChromeWallpaperManager::ProcessRequest(
    ash::sysui::mojom::WallpaperManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ChromeWallpaperManager::Open() {
  wallpaper_manager_util::OpenWallpaperManager();
}
