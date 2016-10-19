// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "components/wallpaper/wallpaper_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

class WallpaperManager
    : public wallpaper::WallpaperManagerBase,
      public ash::mojom::WallpaperManager,
      public content::NotificationObserver,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  class PendingWallpaper;

  ~WallpaperManager() override;

  // Creates an instance of Wallpaper Manager. If there is no instance, create
  // one. Otherwise, returns the existing instance.
  static void Initialize();

  // Gets pointer to singleton WallpaperManager instance.
  static WallpaperManager* Get();

  // Deletes the existing instance of WallpaperManager. Allows the
  // WallpaperManager to remove any observers it has registered.
  static void Shutdown();

  // Binds the mojom::WallpaperManager interface request to this object.
  void BindRequest(ash::mojom::WallpaperManagerRequest request);

  // wallpaper::WallpaperManagerBase:
  WallpaperResolution GetAppropriateResolution() override;
  void AddObservers() override;
  void EnsureLoggedInUserWallpaperLoaded() override;
  void InitializeWallpaper() override;
  void RemoveUserWallpaperInfo(const AccountId& account_id) override;
  void OnPolicyFetched(const std::string& policy,
                       const AccountId& account_id,
                       std::unique_ptr<std::string> data) override;
  void SetCustomWallpaper(const AccountId& account_id,
                          const wallpaper::WallpaperFilesId& wallpaper_files_id,
                          const std::string& file,
                          wallpaper::WallpaperLayout layout,
                          user_manager::User::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool update_wallpaper) override;
  void SetDefaultWallpaperNow(const AccountId& account_id) override;
  void SetDefaultWallpaperDelayed(const AccountId& account_id) override;
  void DoSetDefaultWallpaper(
      const AccountId& account_id,
      wallpaper::MovableOnDestroyCallbackHolder on_finish) override;
  void SetUserWallpaperInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent) override;
  void ScheduleSetUserWallpaper(const AccountId& account_id,
                                bool delayed) override;
  void SetWallpaperFromImageSkia(const AccountId& account_id,
                                 const gfx::ImageSkia& image,
                                 wallpaper::WallpaperLayout layout,
                                 bool update_wallpaper) override;
  void UpdateWallpaper(bool clear_cache) override;
  size_t GetPendingListSizeForTesting() const override;
  wallpaper::WallpaperFilesId GetFilesId(
      const AccountId& account_id) const override;

  // ash::mojom::WallpaperManager:
  void Open() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void UserChangedChildStatus(user_manager::User* user) override;

 private:
  friend class TestApi;
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  WallpaperManager();

  // Returns modifiable PendingWallpaper.
  // Returns pending_inactive_ or creates new PendingWallpaper if necessary.
  PendingWallpaper* GetPendingWallpaper(const AccountId& account_id,
                                        bool delayed);

  // This is called by PendingWallpaper when load is finished.
  void RemovePendingWallpaperFromList(PendingWallpaper* pending);

  // Set wallpaper to |user_image| controlled by policy.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void SetPolicyControlledWallpaper(
      const AccountId& account_id,
      std::unique_ptr<user_manager::UserImage> user_image);

  // wallpaper::WallpaperManagerBase:
  void InitializeRegisteredDeviceWallpaper() override;
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            wallpaper::WallpaperInfo* info) const override;
  void OnWallpaperDecoded(
      const AccountId& account_id,
      wallpaper::WallpaperLayout layout,
      bool update_wallpaper,
      wallpaper::MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image) override;
  void StartLoad(const AccountId& account_id,
                 const wallpaper::WallpaperInfo& info,
                 bool update_wallpaper,
                 const base::FilePath& wallpaper_path,
                 wallpaper::MovableOnDestroyCallbackHolder on_finish) override;
  void SetCustomizedDefaultWallpaperAfterCheck(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files)
      override;
  void OnCustomizedDefaultWallpaperResized(
      const GURL& wallpaper_url,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      std::unique_ptr<bool> success,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) override;
  void SetDefaultWallpaperPathsFromCommandLine(
      base::CommandLine* command_line) override;
  void OnDefaultWallpaperDecoded(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      std::unique_ptr<user_manager::UserImage>* result,
      wallpaper::MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image) override;
  void StartLoadAndSetDefaultWallpaper(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      wallpaper::MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage>* result_out) override;
  void SetDefaultWallpaperPath(
      const base::FilePath& customized_default_wallpaper_file_small,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image) override;

  mojo::BindingSet<ash::mojom::WallpaperManager> bindings_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  // Pointer to last inactive (waiting) entry of 'loading_' list.
  // NULL when there is no inactive request.
  PendingWallpaper* pending_inactive_;

  // Owns PendingWallpaper.
  // PendingWallpaper deletes itself from here on load complete.
  // All pending will be finally deleted on destroy.
  typedef std::vector<scoped_refptr<PendingWallpaper>> PendingList;
  PendingList loading_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
