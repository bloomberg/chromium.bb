// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_

#include <string>

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chrome/browser/ui/webui/options/chromeos/set_wallpaper_options_handler.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/gfx/image/image_skia.h"
#include "unicode/timezone.h"

class PrefService;

namespace chromeos {

class UserImage;

// This class maintains wallpapers for users who have logged into this Chrome
// OS device.
class WallpaperManager: public system::TimezoneSettings::Observer,
                        public chromeos::PowerManagerClient::Observer,
                        public content::NotificationObserver {
 public:
  static WallpaperManager* Get();

  WallpaperManager();

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefService* local_state);

  // Adds PowerManagerClient and TimeZoneSettings observers. It needs to be
  // added after PowerManagerClient initialized.
  void AddObservers();

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

  // Fetches |email|'s wallpaper from local disk.
  void FetchCustomWallpaper(const std::string& email);

  // Gets encoded custom wallpaper from cache. Returns true if success.
  bool GetCustomWallpaperFromCache(const std::string& email,
                                   gfx::ImageSkia* wallpaper);

  // Returns wallpaper/thumbnail filepath for the given user.
  FilePath GetWallpaperPathForUser(const std::string& username,
                                   bool is_thumbnail);

  // Gets the thumbnail of custom wallpaper from cache.
  gfx::ImageSkia GetCustomWallpaperThumbnail(const std::string& email);

  // Sets |type| and |index| to the value saved in local state for logged in
  // user.
  void GetLoggedInUserWallpaperProperties(User::WallpaperType* type,
                                          int* index,
                                          base::Time* last_modification_date);

  // Initializes wallpaper. If logged in, loads user's wallpaper. If not logged
  // in, uses a solid color wallpaper. If logged in as a stub user, uses an
  // empty wallpaper.
  void InitializeWallpaper();

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Starts a one shot timer which calls BatchUpdateWallpaper at next midnight.
  // Cancel any previous timer if any.
  void RestartTimer();

  // Tries to load user image from disk; if successful, sets it for the user,
  // and updates Local State.
  void SetUserWallpaperFromFile(const std::string& username,
                                const FilePath& path,
                                ash::WallpaperLayout layout,
                                base::WeakPtr<WallpaperDelegate> delegate);

  // Set |email|'s wallpaper |type|, |index| and local date to local state.
  // When |is_persistent| is false, changes are kept in memory only.
  void SetUserWallpaperProperties(const std::string& email,
                                  User::WallpaperType type,
                                  int index,
                                  bool is_persistent);

  // Sets one of the default wallpapers for the specified user and saves this
  // settings in local state.
  void SetInitialUserWallpaper(const std::string& username, bool is_persistent);

  // Saves |username| selected wallpaper information to local state.
  void SaveUserWallpaperInfo(const std::string& username,
                             const std::string& file_name,
                             ash::WallpaperLayout layout,
                             User::WallpaperType type);

  // Sets last selected user on user pod row.
  void SetLastSelectedUser(const std::string& last_selected_user);

  // Sets |email|'s wallpaper.
  void SetUserWallpaper(const std::string& email);

  // Sets wallpaper to |wallpaper|.
  void SetWallpaperFromImageSkia(const gfx::ImageSkia& wallpaper,
                                 ash::WallpaperLayout layout);

  // User was deselected at login screen, reset wallpaper if needed.
  void OnUserDeselected() {}

  // User |email| was selected at login screen, load wallpaper.
  void OnUserSelected(const std::string& email);

 private:
  typedef std::map<std::string, gfx::ImageSkia> CustomWallpaperMap;

  virtual ~WallpaperManager();

  // Change the wallpapers for users who choose DAILY wallpaper type. Updates
  // current wallpaper if it changed. This function should be called at exactly
  // at 0am if chromeos device is on.
  void BatchUpdateWallpaper();

  // Cache all logged in users' wallpapers to memory at login screen. It should
  // not compete with first wallpaper loading when boot up/initialize login
  // WebUI page.
  // There are two ways the first wallpaper might be loaded:
  // 1. Loaded on boot. Login WebUI waits for it.
  // 2. When flag --disable-boot-animation is passed. Login WebUI is loaded
  // right away and in 500ms after. Wallpaper started to load.
  // For case 2, should_cache_wallpaper_ is used to indicate if we need to
  // cache wallpapers on wallpaper animation finished. The cache operation
  // should be only executed once.
  void CacheAllUsersWallpapers();

  // Caches |email|'s wallpaper to memory.
  void CacheUserWallpaper(const std::string& email);

  // Caches the decoded wallpaper to memory.
  void CacheWallpaper(const std::string& email, const UserImage& wallpaper);

  // Generates a 128x80 thumbnail and caches it.
  void CacheThumbnail(const std::string& email,
                      const gfx::ImageSkia& wallpaper);

  // Sets wallpaper to the decoded wallpaper.
  void FetchWallpaper(const std::string& email,
                      ash::WallpaperLayout layout,
                      const UserImage& wallpaper);

  // Generates a 128x80 thumbnail and notifies delegate when ready.
  void GenerateUserWallpaperThumbnail(const std::string& email,
                                      User::WallpaperType type,
                                      base::WeakPtr<WallpaperDelegate> delegate,
                                      const gfx::ImageSkia& wallpaper);

  // Get wallpaper |type|, |index| and |last_modification_date| of |email|
  // from local state.
  void GetUserWallpaperProperties(const std::string& email,
                                  User::WallpaperType* type,
                                  int* index,
                                  base::Time* last_modification_date);

  // Updates the custom wallpaper thumbnail in wallpaper picker UI.
  void OnThumbnailUpdated(base::WeakPtr<WallpaperDelegate> delegate);

  // Saves wallpaper to |path|.
  void SaveWallpaper(const std::string& path, const UserImage& wallpaper);

  // Saves wallpaper to file, post task to generate thumbnail and updates local
  // state preferences.
  void SetWallpaper(const std::string& username,
                    ash::WallpaperLayout layout,
                    User::WallpaperType type,
                    base::WeakPtr<WallpaperDelegate> delegate,
                    const UserImage& wallpaper);

  // Whether wallpaper data should be persisted for user |email|.
  // Note: this function can not be called in SetUserWallpaperProperties. It
  // will create a deadlock. (issue 142440)
  bool ShouldPersistDataForUser(const std::string& email);

  // Sets wallpaper to image in |user_image| with |layout|.
  void OnWallpaperLoaded(ash::WallpaperLayout layout,
                         const UserImage& user_image);

  // Overridden from chromeos::ResumeObserver
  virtual void SystemResumed() OVERRIDE;

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // Loads user wallpaper from its file.
  scoped_refptr<UserImageLoader> wallpaper_loader_;

  // Logged-in user wallpaper type.
  User::WallpaperType current_user_wallpaper_type_;

  // Logged-in user wallpaper index.
  int current_user_wallpaper_index_;

  // Caches custom wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap custom_wallpaper_cache_;

  CustomWallpaperMap custom_wallpaper_thumbnail_cache_;

  // The last selected user on user pod row.
  std::string last_selected_user_;

  bool should_cache_wallpaper_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;

  content::NotificationRegistrar registrar_;

  base::OneShotTimer<WallpaperManager> timer_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
