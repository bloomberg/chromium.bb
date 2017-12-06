// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/wallpaper_controller_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_files_id.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

// Known user keys.
const char kWallpaperFilesId[] = "wallpaper-files-id";

WallpaperControllerClient* g_instance = nullptr;

// Creates a mojom::WallpaperUserInfo for the account id. Returns nullptr if
// user manager cannot find the user.
ash::mojom::WallpaperUserInfoPtr AccountIdToWallpaperUserInfo(
    const AccountId& account_id) {
  if (!account_id.is_valid()) {
    // |account_id| may be invalid in tests.
    return nullptr;
  }
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

// This has once been copied from
// brillo::cryptohome::home::SanitizeUserName(username) to be used for
// wallpaper identification purpose only.
//
// Historic note: We need some way to identify users wallpaper files in
// the device filesystem. Historically User::username_hash() was used for this
// purpose, but it has two caveats:
// 1. username_hash() is defined only after user has logged in.
// 2. If cryptohome identifier changes, username_hash() will also change,
//    and we may lose user => wallpaper files mapping at that point.
// So this function gives WallpaperManager independent hashing method to break
// this dependency.
wallpaper::WallpaperFilesId HashWallpaperFilesIdStr(
    const std::string& files_id_unhashed) {
  chromeos::SystemSaltGetter* salt_getter = chromeos::SystemSaltGetter::Get();
  DCHECK(salt_getter);

  // System salt must be defined at this point.
  const chromeos::SystemSaltGetter::RawSalt* salt = salt_getter->GetRawSalt();
  if (!salt)
    LOG(FATAL) << "WallpaperManager HashWallpaperFilesIdStr(): no salt!";

  unsigned char binmd[base::kSHA1Length];
  std::string lowercase(files_id_unhashed);
  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 ::tolower);
  std::vector<uint8_t> data = *salt;
  std::copy(files_id_unhashed.begin(), files_id_unhashed.end(),
            std::back_inserter(data));
  base::SHA1HashBytes(data.data(), data.size(), binmd);
  std::string result = base::HexEncode(binmd, sizeof(binmd));
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  return wallpaper::WallpaperFilesId::FromString(result);
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

bool WallpaperControllerClient::CanGetWallpaperFilesId() const {
  return chromeos::SystemSaltGetter::IsInitialized() &&
         chromeos::SystemSaltGetter::Get()->GetRawSalt();
}

wallpaper::WallpaperFilesId WallpaperControllerClient::GetFilesId(
    const AccountId& account_id) const {
  // System salt might not be ready in tests. Thus we don't have a valid
  // wallpaper files id here.
  if (!CanGetWallpaperFilesId())
    return wallpaper::WallpaperFilesId();

  std::string stored_value;
  if (user_manager::known_user::GetStringPref(account_id, kWallpaperFilesId,
                                              &stored_value)) {
    return wallpaper::WallpaperFilesId::FromString(stored_value);
  }

  // Migrated.
  const std::string& old_id = account_id.GetUserEmail();
  const wallpaper::WallpaperFilesId wallpaper_files_id =
      HashWallpaperFilesIdStr(old_id);
  user_manager::known_user::SetStringPref(account_id, kWallpaperFilesId,
                                          wallpaper_files_id.id());
  return wallpaper_files_id;
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
  wallpaper_controller_->RemoveUserWallpaper(std::move(user_info),
                                             GetFilesId(account_id).id());
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

  // Get the paths of wallpaper directories.
  base::FilePath user_data_path;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &user_data_path));
  base::FilePath chromeos_wallpapers_path;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_WALLPAPERS,
                         &chromeos_wallpapers_path));
  base::FilePath chromeos_custom_wallpapers_path;
  CHECK(PathService::Get(chrome::DIR_CHROMEOS_CUSTOM_WALLPAPERS,
                         &chromeos_custom_wallpapers_path));

  wallpaper_controller_->SetClientAndPaths(std::move(client), user_data_path,
                                           chromeos_wallpapers_path,
                                           chromeos_custom_wallpapers_path);
}
