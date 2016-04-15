// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_

#include <stddef.h>

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "components/wallpaper/wallpaper_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

class WallpaperManager
    : public wallpaper::WallpaperManagerBase,
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

  // Returns the appropriate wallpaper resolution for all root windows.
  WallpaperResolution GetAppropriateResolution() override;

  // Adds PowerManagerClient, TimeZoneSettings and CrosSettings observers.
  void AddObservers() override;

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded() override;

  // Initializes wallpaper. If logged in, loads user's wallpaper. If not logged
  // in, uses a solid color wallpaper. If logged in as a stub user, uses an
  // empty wallpaper.
  void InitializeWallpaper() override;

  // NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Removes all |account_id| related wallpaper info and saved wallpapers.
  void RemoveUserWallpaperInfo(const AccountId& account_id) override;

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  void OnPolicyFetched(const std::string& policy,
                       const AccountId& account_id,
                       std::unique_ptr<std::string> data) override;

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences. If |update_wallpaper| is false, don't change
  // wallpaper but only update cache.
  void SetCustomWallpaper(const AccountId& account_id,
                          const wallpaper::WallpaperFilesId& wallpaper_files_id,
                          const std::string& file,
                          wallpaper::WallpaperLayout layout,
                          user_manager::User::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool update_wallpaper) override;

  // Sets wallpaper to default wallpaper (asynchronously with zero delay).
  void SetDefaultWallpaperNow(const AccountId& account_id) override;

  // Sets wallpaper to default wallpaper (asynchronously with default delay).
  void SetDefaultWallpaperDelayed(const AccountId& account_id) override;

  // Sets wallpaper to default.
  void DoSetDefaultWallpaper(
      const AccountId& account_id,
      wallpaper::MovableOnDestroyCallbackHolder on_finish) override;

  // Sets selected wallpaper information for |account_id| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent) override;

  // Creates new PendingWallpaper request (or updates currently pending).
  void ScheduleSetUserWallpaper(const AccountId& account_id,
                                bool delayed) override;

  // Sets wallpaper to |image| (asynchronously with zero delay). If
  // |update_wallpaper| is false, skip change wallpaper but only update cache.
  void SetWallpaperFromImageSkia(const AccountId& account_id,
                                 const gfx::ImageSkia& image,
                                 wallpaper::WallpaperLayout layout,
                                 bool update_wallpaper) override;

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution. (asynchronously with zero delay)
  void UpdateWallpaper(bool clear_cache) override;

  // Returns queue size.
  size_t GetPendingListSizeForTesting() const override;

  // Returns wallpaper files id for the |account_id|.
  wallpaper::WallpaperFilesId GetFilesId(
      const AccountId& account_id) const override;

  // Overridden from user_manager::UserManager::UserSessionStateObserver:
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

  // WallpaperManagerBase overrides:
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

  std::unique_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  // Pointer to last inactive (waiting) entry of 'loading_' list.
  // NULL when there is no inactive request.
  PendingWallpaper* pending_inactive_;

  // Owns PendingWallpaper.
  // PendingWallpaper deletes itself from here on load complete.
  // All pending will be finally deleted on destroy.
  typedef std::vector<scoped_refptr<PendingWallpaper> > PendingList;
  PendingList loading_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
