// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_

#include <string>

#include "ash/desktop_background/desktop_background_resources.h"
#include "base/memory/ref_counted_memory.h"
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

struct WallpaperInfo {
  // Online wallpaper URL or file name of migrated wallpaper.
  std::string file;
  ash::WallpaperLayout layout;
  User::WallpaperType type;
  base::Time date;
  bool operator==(const WallpaperInfo& other) {
    return (file == other.file) && (layout == other.layout) &&
        (type == other.type);
  }
};

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

  // Gets |email|'s wallpaper from local disk. When |update_wallpaper| is true,
  // sets wallpaper to the loaded wallpaper
  void GetCustomWallpaper(const std::string& email, bool update_wallpaper);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const std::string& email,
                             gfx::ImageSkia* wallpaper);

  // Returns filepath to save original custom wallpaper for the given user.
  FilePath GetOriginalWallpaperPathForUser(const std::string& username);

  // Returns small resolution custom wallpaper filepath for the given user when
  // |is_small| is ture. Otherwise, returns large resolution custom wallpaper
  // path.
  FilePath GetWallpaperPathForUser(const std::string& username,
                                   bool is_small);

  // Gets the thumbnail of custom wallpaper from cache.
  gfx::ImageSkia GetCustomWallpaperThumbnail(const std::string& email);

  // Gets wallpaper properties of logged in user.
  void GetLoggedInUserWallpaperProperties(User::WallpaperType* type,
                                          int* index,
                                          base::Time* last_modification_date);

  // Gets wallpaper information of logged in user.
  bool GetLoggedInUserWallpaperInfo(WallpaperInfo* info);

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

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences.
  void SetCustomWallpaper(const std::string& username,
                          ash::WallpaperLayout layout,
                          User::WallpaperType type,
                          base::WeakPtr<WallpaperDelegate> delegate,
                          const UserImage& wallpaper);

  // Tries to load user image from disk; if successful, sets it for the user,
  // and updates Local State.
  void SetUserWallpaperFromFile(const std::string& username,
                                const FilePath& path,
                                ash::WallpaperLayout layout,
                                base::WeakPtr<WallpaperDelegate> delegate);

  // Sets selected wallpaper properties for |username| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperProperties(const std::string& email,
                                  User::WallpaperType type,
                                  int index,
                                  bool is_persistent);

  // Sets one of the default wallpapers for the specified user and saves this
  // settings in local state.
  void SetInitialUserWallpaper(const std::string& username, bool is_persistent);

  // Sets selected wallpaper information for |username| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperInfo(const std::string& username,
                            const WallpaperInfo& info,
                            bool is_persistent);

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

  // Generates a 128x80 thumbnail and caches it.
  void CacheThumbnail(const std::string& email,
                      const gfx::ImageSkia& wallpaper);

  // Loads |email|'s wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper.
  void LoadWallpaper(const std::string& email,
                     const WallpaperInfo& info,
                     bool update_wallpaper);

  // Generates a 128x80 thumbnail and notifies delegate when ready.
  void GenerateUserWallpaperThumbnail(const std::string& email,
                                      User::WallpaperType type,
                                      base::WeakPtr<WallpaperDelegate> delegate,
                                      const gfx::ImageSkia& wallpaper);

  // Gets |email|'s custom wallpaper in appropriate resolution. Falls back on
  // original custom wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper. Must run on FILE thread.
  void GetCustomWallpaperInternal(const std::string& email,
                                  const WallpaperInfo& info,
                                  bool update_wallpaper);

  // Gets wallpaper information of |email| from Local State or memory. Returns
  // false if wallpaper information is not found.
  bool GetUserWallpaperInfo(const std::string& email, WallpaperInfo* info);

  // Gets wallpaper |type|, |index| and |last_modification_date| of |email|
  // from local state.
  void GetUserWallpaperProperties(const std::string& email,
                                  User::WallpaperType* type,
                                  int* index,
                                  base::Time* last_modification_date);

  // Copy |email| selected built-in wallpapers (high and low resolution) in
  // ChromeOS image binary to local files on disk. Also converts the wallpaper
  // properties correspond to the built-in wallpapers to the new wallpaper info.
  void MigrateBuiltInWallpaper(const std::string& email);

  // Updates the custom wallpaper thumbnail in wallpaper picker UI.
  void OnThumbnailUpdated(base::WeakPtr<WallpaperDelegate> delegate);

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.
  void OnWallpaperDecoded(const std::string& email,
                          ash::WallpaperLayout layout,
                          bool update_wallpaper,
                          const UserImage& wallpaper);

  // Saves encoded wallpaper to |path|. This callback is called after encoding
  // to PNG completes.
  void OnWallpaperEncoded(const FilePath& path,
                          scoped_refptr<base::RefCountedBytes> data);

  // Resizes |wallpaper| to a resolution which is nearest to |preferred_width|
  // and |preferred_height| while maintaining aspect ratio. And saves the
  // resized wallpaper to |path|.
  void ResizeAndSaveCustomWallpaper(const UserImage& wallpaper,
                                    const FilePath& path,
                                    ash::WallpaperLayout layout,
                                    int preferred_width,
                                    int preferred_height);

  // Record data for User Metrics Analysis.
  void RecordUma(User::WallpaperType type, int index);

  // Saves original custom wallpaper to |path| (absolute path) on filesystem
  // and starts resizing operation of the custom wallpaper if necessary.
  void SaveCustomWallpaper(const std::string& email,
                           const FilePath& path,
                           ash::WallpaperLayout layout,
                           const UserImage& wallpaper);

  // Saves wallpaper image raw |data| to |path| (absolute path) in file system.
  void SaveWallpaperInternal(const FilePath& path, const char* data, int size);

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

  // Logged-in user wallpaper information.
  WallpaperInfo current_user_wallpaper_info_;

  // Caches wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap wallpaper_cache_;

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
