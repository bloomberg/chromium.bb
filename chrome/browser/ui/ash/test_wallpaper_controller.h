// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Simulates WallpaperController in ash.
// TODO(crbug.com/776464): Maybe create an enum to represent each function to
// avoid having lots of counters and getters.
class TestWallpaperController : ash::mojom::WallpaperController {
 public:
  TestWallpaperController();

  ~TestWallpaperController() override;

  void ClearCounts();

  bool was_client_set() const { return was_client_set_; }

  int remove_user_wallpaper_count() { return remove_user_wallpaper_count_; }

  // Returns a mojo interface pointer bound to this object.
  ash::mojom::WallpaperControllerPtr CreateInterfacePtr();

  // ash::mojom::WallpaperController:
  void SetClientAndPaths(
      ash::mojom::WallpaperControllerClientPtr client,
      const base::FilePath& user_data_path,
      const base::FilePath& chromeos_wallpapers_path,
      const base::FilePath& chromeos_custom_wallpapers_path) override;
  void SetCustomWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                          const std::string& wallpaper_files_id,
                          const std::string& file_name,
                          wallpaper::WallpaperLayout layout,
                          wallpaper::WallpaperType type,
                          const SkBitmap& image,
                          bool show_wallpaper) override;
  void SetOnlineWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                          const SkBitmap& image,
                          const std::string& url,
                          wallpaper::WallpaperLayout layout,
                          bool show_wallpaper) override;
  void SetDefaultWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                           bool show_wallpaper) override;
  void SetCustomizedDefaultWallpaper(
      const GURL& wallpaper_url,
      const base::FilePath& file_path,
      const base::FilePath& resized_directory) override;
  void ShowUserWallpaper(ash::mojom::WallpaperUserInfoPtr user_info) override;
  void ShowSigninWallpaper() override;
  void RemoveUserWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id) override;
  void SetWallpaper(const SkBitmap& wallpaper,
                    const wallpaper::WallpaperInfo& wallpaper_info) override;
  void AddObserver(
      ash::mojom::WallpaperObserverAssociatedPtrInfo observer) override;
  void GetWallpaperColors(GetWallpaperColorsCallback callback) override;

 private:
  mojo::Binding<ash::mojom::WallpaperController> binding_;

  bool was_client_set_ = false;

  int remove_user_wallpaper_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperController);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_
