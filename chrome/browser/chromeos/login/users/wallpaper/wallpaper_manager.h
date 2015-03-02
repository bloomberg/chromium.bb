// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_

#include <deque>
#include <string>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/login/users/avatar/user_image_loader.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/wallpaper/wallpaper_layout.h"
#include "components/wallpaper/wallpaper_manager_base.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

class WallpaperManager : public wallpaper::WallpaperManagerBase {
 public:
  class PendingWallpaper;

  WallpaperManager();
  ~WallpaperManager() override;

  // Get pointer to singleton WallpaperManager instance, create it if necessary.
  static WallpaperManager* Get();

  // Returns the appropriate wallpaper resolution for all root windows.
  WallpaperResolution GetAppropriateResolution() override;

  // Indicates imminent shutdown, allowing the WallpaperManager to remove any
  // observers it has registered.
  void Shutdown() override;

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

  // Removes all |user_id| related wallpaper info and saved wallpapers.
  void RemoveUserWallpaperInfo(const std::string& user_id) override;

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  void OnPolicyFetched(const std::string& policy,
                       const std::string& user_id,
                       scoped_ptr<std::string> data) override;

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences. If |update_wallpaper| is false, don't change
  // wallpaper but only update cache.
  void SetCustomWallpaper(const std::string& user_id,
                          const std::string& user_id_hash,
                          const std::string& file,
                          wallpaper::WallpaperLayout layout,
                          user_manager::User::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool update_wallpaper) override;

  // Sets wallpaper to default wallpaper (asynchronously with zero delay).
  void SetDefaultWallpaperNow(const std::string& user_id) override;

  // Sets wallpaper to default wallpaper (asynchronously with default delay).
  void SetDefaultWallpaperDelayed(const std::string& user_id) override;

  // Sets wallpaper to default.
  void DoSetDefaultWallpaper(
      const std::string& user_id,
      wallpaper::MovableOnDestroyCallbackHolder on_finish) override;

  // Sets selected wallpaper information for |user_id| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperInfo(const std::string& user_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent) override;

  // Creates new PendingWallpaper request (or updates currently pending).
  void ScheduleSetUserWallpaper(const std::string& user_id,
                                bool delayed) override;

  // Sets wallpaper to |image| (asynchronously with zero delay). If
  // |update_wallpaper| is false, skip change wallpaper but only update cache.
  void SetWallpaperFromImageSkia(const std::string& user_id,
                                 const gfx::ImageSkia& image,
                                 wallpaper::WallpaperLayout layout,
                                 bool update_wallpaper) override;

  // Returns queue size.
  size_t GetPendingListSizeForTesting() const override;

 private:
  friend class TestApi;
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  // Returns modifiable PendingWallpaper.
  // Returns pending_inactive_ or creates new PendingWallpaper if necessary.
  PendingWallpaper* GetPendingWallpaper(const std::string& user_id,
                                        bool delayed);

  // This is called by PendingWallpaper when load is finished.
  void RemovePendingWallpaperFromList(PendingWallpaper* pending);

  // WallpaperManagerBase overrides:
  void ClearObsoleteWallpaperPrefs() override;
  void InitializeRegisteredDeviceWallpaper() override;
  bool GetUserWallpaperInfo(const std::string& user_id,
                            wallpaper::WallpaperInfo* info) const override;
  void OnWallpaperDecoded(const std::string& user_id,
                          wallpaper::WallpaperLayout layout,
                          bool update_wallpaper,
                          wallpaper::MovableOnDestroyCallbackHolder on_finish,
                          const user_manager::UserImage& user_image) override;
  void StartLoad(const std::string& user_id,
                 const wallpaper::WallpaperInfo& info,
                 bool update_wallpaper,
                 const base::FilePath& wallpaper_path,
                 wallpaper::MovableOnDestroyCallbackHolder on_finish) override;
  void SetCustomizedDefaultWallpaperAfterCheck(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files) override;
  void OnCustomizedDefaultWallpaperResized(
      const GURL& wallpaper_url,
      scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      scoped_ptr<bool> success,
      scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
      scoped_ptr<gfx::ImageSkia> large_wallpaper_image) override;
  void OnDefaultWallpaperDecoded(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      scoped_ptr<user_manager::UserImage>* result,
      wallpaper::MovableOnDestroyCallbackHolder on_finish,
      const user_manager::UserImage& user_image) override;
  void StartLoadAndSetDefaultWallpaper(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      wallpaper::MovableOnDestroyCallbackHolder on_finish,
      scoped_ptr<user_manager::UserImage>* result_out) override;
  void SetDefaultWallpaperPath(
      const base::FilePath& customized_default_wallpaper_file_small,
      scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      scoped_ptr<gfx::ImageSkia> large_wallpaper_image) override;

  // Loads user wallpaper from its file.
  scoped_refptr<UserImageLoader> wallpaper_loader_;

  scoped_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  // Pointer to last inactive (waiting) entry of 'loading_' list.
  // NULL when there is no inactive request.
  PendingWallpaper* pending_inactive_;

  // Owns PendingWallpaper.
  // PendingWallpaper deletes itself from here on load complete.
  // All pending will be finally deleted on destroy.
  typedef std::vector<scoped_refptr<PendingWallpaper> > PendingList;
  PendingList loading_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
