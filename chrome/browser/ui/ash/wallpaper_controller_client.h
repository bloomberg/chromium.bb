// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/wallpaper_policy_handler.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace wallpaper {
class WallpaperFilesId;
enum WallpaperLayout;
enum WallpaperType;
}  // namespace wallpaper

// Handles method calls sent from ash to chrome. Also sends messages from chrome
// to ash.
class WallpaperControllerClient : public ash::mojom::WallpaperControllerClient,
                                  public WallpaperPolicyHandler::Delegate {
 public:
  WallpaperControllerClient();
  ~WallpaperControllerClient() override;

  // Initializes and connects to ash.
  void Init();

  // Tests can provide a mock mojo interface for the ash controller.
  void InitForTesting(ash::mojom::WallpaperControllerPtr controller);

  static WallpaperControllerClient* Get();

  // TODO(crbug.com/776464): Move this to anonymous namesapce.
  // Returns true if wallpaper files id can be returned successfully.
  bool CanGetWallpaperFilesId() const;

  // Returns files identifier for the |account_id|.
  wallpaper::WallpaperFilesId GetFilesId(const AccountId& account_id) const;

  // Wrappers around the ash::mojom::WallpaperController interface.
  void SetCustomWallpaper(const AccountId& account_id,
                          const wallpaper::WallpaperFilesId& wallpaper_files_id,
                          const std::string& file_name,
                          wallpaper::WallpaperLayout layout,
                          wallpaper::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool show_wallpaper);
  void SetOnlineWallpaper(const AccountId& account_id,
                          const gfx::ImageSkia& image,
                          const std::string& url,
                          wallpaper::WallpaperLayout layout,
                          bool show_wallpaper);
  void SetDefaultWallpaper(const AccountId& account_id, bool show_wallpaper);
  void SetCustomizedDefaultWallpaper(const GURL& wallpaper_url,
                                     const base::FilePath& file_path,
                                     const base::FilePath& resized_directory);
  void ShowUserWallpaper(const AccountId& account_id);
  void ShowSigninWallpaper();
  void RemoveUserWallpaper(const AccountId& account_id);

  // ash::mojom::WallpaperControllerClient:
  void OpenWallpaperPicker() override;

  // chromeos::WallpaperPolicyHandler::Delegate:
  void OnDeviceWallpaperChanged() override;
  void OnDeviceWallpaperPolicyCleared() override;

  // Flushes the mojo pipe to ash.
  void FlushForTesting();

 private:
  // Binds this object to its mojo interface and sets it as the ash client.
  void BindAndSetClient();

  // WallpaperController interface in ash.
  ash::mojom::WallpaperControllerPtr wallpaper_controller_;

  // Binds to the client interface.
  mojo::Binding<ash::mojom::WallpaperControllerClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperControllerClient);
};

#endif  // CHROME_BROWSER_UI_ASH_WALLPAPER_CONTROLLER_CLIENT_H_
