// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

WallpaperControllerClient* g_instance = nullptr;

// Creates a mojom::WallpaperUserInfo for the account id. Returns nullptr if
// user manager cannot find the user.
ash::mojom::WallpaperUserInfoPtr AccountIdToWallpaperUserInfo(
    const AccountId& account_id) {
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user)
    return nullptr;

  ash::mojom::WallpaperUserInfoPtr wallpaper_user_info =
      ash::mojom::WallpaperUserInfo::New();
  wallpaper_user_info->account_id = account_id;
  wallpaper_user_info->type = user->GetType();
  wallpaper_user_info->is_ephemeral =
      user_manager::UserManager::Get()->IsUserNonCryptohomeDataEphemeral(
          account_id);
  wallpaper_user_info->has_gaia_account = user->HasGaiaAccount();

  return wallpaper_user_info;
}

}  // namespace

WallpaperControllerClient::WallpaperControllerClient() : binding_(this) {
  DCHECK(!g_instance);
  g_instance = this;
}

WallpaperControllerClient::~WallpaperControllerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void WallpaperControllerClient::Init() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &wallpaper_controller_);
  BindAndSetClient();
}

void WallpaperControllerClient::InitForTesting(
    ash::mojom::WallpaperControllerPtr controller) {
  wallpaper_controller_ = std::move(controller);
  BindAndSetClient();
}

// static
WallpaperControllerClient* WallpaperControllerClient::Get() {
  return g_instance;
}

void WallpaperControllerClient::SetCustomWallpaper(
    const AccountId& account_id,
    const wallpaper::WallpaperFilesId& wallpaper_files_id,
    const std::string& file_name,
    wallpaper::WallpaperLayout layout,
    wallpaper::WallpaperType type,
    const gfx::ImageSkia& image,
    bool show_wallpaper) {
  ash::mojom::WallpaperUserInfoPtr user_info =
      AccountIdToWallpaperUserInfo(account_id);
  if (!user_info)
    return;
  wallpaper_controller_->SetCustomWallpaper(
      std::move(user_info), wallpaper_files_id.id(), file_name, layout, type,
      *image.bitmap(), show_wallpaper);
}

void WallpaperControllerClient::SetOnlineWallpaper(
    const AccountId& account_id,
    const gfx::ImageSkia& image,
    const std::string& url,
    wallpaper::WallpaperLayout layout,
    bool show_wallpaper) {
  ash::mojom::WallpaperUserInfoPtr user_info =
      AccountIdToWallpaperUserInfo(account_id);
  if (!user_info)
    return;
  wallpaper_controller_->SetOnlineWallpaper(
      std::move(user_info), *image.bitmap(), url, layout, show_wallpaper);
}

void WallpaperControllerClient::SetDefaultWallpaper(const AccountId& account_id,
                                                    bool show_wallpaper) {
  ash::mojom::WallpaperUserInfoPtr user_info =
      AccountIdToWallpaperUserInfo(account_id);
  if (!user_info)
    return;
  wallpaper_controller_->SetDefaultWallpaper(std::move(user_info),
                                             show_wallpaper);
}

void WallpaperControllerClient::SetCustomizedDefaultWallpaper(
    const GURL& wallpaper_url,
    const base::FilePath& file_path,
    const base::FilePath& resized_directory) {
  wallpaper_controller_->SetCustomizedDefaultWallpaper(wallpaper_url, file_path,
                                                       resized_directory);
}

void WallpaperControllerClient::ShowUserWallpaper(const AccountId& account_id) {
  ash::mojom::WallpaperUserInfoPtr user_info =
      AccountIdToWallpaperUserInfo(account_id);
  if (!user_info)
    return;
  wallpaper_controller_->ShowUserWallpaper(std::move(user_info));
}

void WallpaperControllerClient::ShowSigninWallpaper() {
  wallpaper_controller_->ShowSigninWallpaper();
}

void WallpaperControllerClient::RemoveUserWallpaper(
    const AccountId& account_id) {
  ash::mojom::WallpaperUserInfoPtr user_info =
      AccountIdToWallpaperUserInfo(account_id);
  if (!user_info)
    return;
  wallpaper_controller_->RemoveUserWallpaper(std::move(user_info));
}

void WallpaperControllerClient::OpenWallpaperPicker() {
  // TODO(crbug.com/776464): Inline the implementation after WallpaperManager
  // is removed.
  chromeos::WallpaperManager::Get()->OpenWallpaperPicker();
}

void WallpaperControllerClient::FlushForTesting() {
  wallpaper_controller_.FlushForTesting();
}

void WallpaperControllerClient::BindAndSetClient() {
  ash::mojom::WallpaperControllerClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  wallpaper_controller_->SetClient(std::move(client));
}
