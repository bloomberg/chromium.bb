// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_wallpaper_controller.h"

TestWallpaperController::TestWallpaperController() : binding_(this) {}

TestWallpaperController::~TestWallpaperController() = default;

void TestWallpaperController::ClearCounts() {
  remove_user_wallpaper_count_ = 0;
}

ash::mojom::WallpaperControllerPtr
TestWallpaperController::CreateInterfacePtr() {
  ash::mojom::WallpaperControllerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestWallpaperController::SetClientAndPaths(
    ash::mojom::WallpaperControllerClientPtr client,
    const base::FilePath& user_data_path,
    const base::FilePath& chromeos_wallpapers_path,
    const base::FilePath& chromeos_custom_wallpapers_path) {
  was_client_set_ = true;
}

void TestWallpaperController::SetCustomWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id,
    const std::string& file_name,
    wallpaper::WallpaperLayout layout,
    wallpaper::WallpaperType type,
    const SkBitmap& image,
    bool show_wallpaper) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetOnlineWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const SkBitmap& image,
    const std::string& url,
    wallpaper::WallpaperLayout layout,
    bool show_wallpaper) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetDefaultWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    bool show_wallpaper) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    const base::FilePath& resized_directory) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowUserWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::ShowSigninWallpaper() {
  NOTIMPLEMENTED();
}

void TestWallpaperController::RemoveUserWallpaper(
    ash::mojom::WallpaperUserInfoPtr user_info,
    const std::string& wallpaper_files_id) {
  remove_user_wallpaper_count_++;
}

void TestWallpaperController::SetWallpaper(
    const SkBitmap& wallpaper,
    const wallpaper::WallpaperInfo& wallpaper_info) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::AddObserver(
    ash::mojom::WallpaperObserverAssociatedPtrInfo observer) {
  NOTIMPLEMENTED();
}

void TestWallpaperController::GetWallpaperColors(
    GetWallpaperColorsCallback callback) {
  NOTIMPLEMENTED();
}
