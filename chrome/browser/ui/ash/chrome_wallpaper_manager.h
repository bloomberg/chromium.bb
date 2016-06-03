// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_CHROME_WALLPAPER_MANAGER_H_

#include "ash/sysui/public/interfaces/wallpaper.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

// ChromeWallpaperManager exposes chrome wallpaper functionality to ash_sysui.
class ChromeWallpaperManager : public ash::sysui::mojom::WallpaperManager {
 public:
  ChromeWallpaperManager();
  ~ChromeWallpaperManager() override;

  void ProcessRequest(ash::sysui::mojom::WallpaperManagerRequest request);

 private:
  // ash::sysui::mojom::WallpaperManager:
  void Open() override;

  mojo::BindingSet<ash::sysui::mojom::WallpaperManager> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeWallpaperManager);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_WALLPAPER_MANAGER_H_
