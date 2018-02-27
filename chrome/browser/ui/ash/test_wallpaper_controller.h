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
  int remove_user_wallpaper_count() const {
    return remove_user_wallpaper_count_;
  }
  int set_default_wallpaper_count() const {
    return set_default_wallpaper_count_;
  }
  int set_custom_wallpaper_count() const { return set_custom_wallpaper_count_; }

  // Returns a mojo interface pointer bound to this object.
  ash::mojom::WallpaperControllerPtr CreateInterfacePtr();

  // ash::mojom::WallpaperController:
  void Init(ash::mojom::WallpaperControllerClientPtr client,
            const base::FilePath& user_data_path,
            const base::FilePath& chromeos_wallpapers_path,
            const base::FilePath& chromeos_custom_wallpapers_path,
            bool is_device_wallpaper_policy_enforced) override;
  void SetCustomWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                          const std::string& wallpaper_files_id,
                          const std::string& file_name,
                          wallpaper::WallpaperLayout layout,
                          const SkBitmap& image,
                          bool show_wallpaper) override;
  void SetOnlineWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                          const SkBitmap& image,
                          const std::string& url,
                          wallpaper::WallpaperLayout layout,
                          bool show_wallpaper) override;
  void SetDefaultWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id,
                           bool show_wallpaper) override;
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_small_path,
      const base::FilePath& customized_default_large_path) override;
  void SetPolicyWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                          const std::string& wallpaper_files_id,
                          const std::string& data) override;
  void SetDeviceWallpaperPolicyEnforced(bool enforced) override;
  void UpdateCustomWallpaperLayout(ash::mojom::WallpaperUserInfoPtr user_info,
                                   wallpaper::WallpaperLayout layout) override;
  void ShowUserWallpaper(ash::mojom::WallpaperUserInfoPtr user_info) override;
  void ShowSigninWallpaper() override;
  void RemoveUserWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id) override;
  void RemovePolicyWallpaper(ash::mojom::WallpaperUserInfoPtr user_info,
                             const std::string& wallpaper_files_id) override;
  void SetAnimationDuration(base::TimeDelta animation_duration) override;
  void OpenWallpaperPickerIfAllowed() override;
  void AddObserver(
      ash::mojom::WallpaperObserverAssociatedPtrInfo observer) override;
  void GetWallpaperColors(GetWallpaperColorsCallback callback) override;
  void IsActiveUserWallpaperControlledByPolicy(
      ash::mojom::WallpaperController::
          IsActiveUserWallpaperControlledByPolicyCallback callback) override;
  void ShouldShowWallpaperSetting(
      ash::mojom::WallpaperController::ShouldShowWallpaperSettingCallback
          callback) override;

 private:
  mojo::Binding<ash::mojom::WallpaperController> binding_;

  bool was_client_set_ = false;
  int remove_user_wallpaper_count_ = 0;
  int set_default_wallpaper_count_ = 0;
  int set_custom_wallpaper_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestWallpaperController);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_WALLPAPER_CONTROLLER_H_
