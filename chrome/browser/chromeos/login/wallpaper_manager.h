// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_

#include <string>

#include "ash/desktop_background/desktop_background_controller.h"
#include "base/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "chrome/browser/chromeos/system/timezone_settings.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/root_power_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/icu/public/i18n/unicode/timezone.h"
#include "ui/gfx/image/image_skia.h"

class PrefServiceSimple;

namespace base {
class SequencedTaskRunner;
}

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

class WallpaperManagerBrowserTest;
class UserImage;

// Name of wallpaper sequence token.
extern const char kWallpaperSequenceTokenName[];

// File path suffices of resized small or large wallpaper.
extern const char kSmallWallpaperSuffix[];
extern const char kLargeWallpaperSuffix[];

// This class maintains wallpapers for users who have logged into this Chrome
// OS device.
class WallpaperManager: public system::TimezoneSettings::Observer,
                        public chromeos::PowerManagerClient::Observer,
                        public chromeos::RootPowerManagerObserver,
                        public content::NotificationObserver {
 public:
  static WallpaperManager* Get();

  WallpaperManager();

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefServiceSimple* local_state);

  // Adds PowerManagerClient and TimeZoneSettings observers. It needs to be
  // added after PowerManagerClient has been initialized.
  void AddObservers();

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

  // Clears ONLINE and CUSTOM wallpaper cache.
  void ClearWallpaperCache();

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

  // Removes all |email| related wallpaper info and saved wallpapers.
  void RemoveUserWallpaperInfo(const std::string& email);

  // Resizes |wallpaper| to a resolution which is nearest to |preferred_width|
  // and |preferred_height| while maintaining aspect ratio. And saves the
  // resized wallpaper to |path|.
  void ResizeAndSaveWallpaper(const UserImage& wallpaper,
                              const FilePath& path,
                              ash::WallpaperLayout layout,
                              int preferred_width,
                              int preferred_height);

  // Starts a one shot timer which calls BatchUpdateWallpaper at next midnight.
  // Cancel any previous timer if any.
  void RestartTimer();

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences.
  void SetCustomWallpaper(const std::string& username,
                          ash::WallpaperLayout layout,
                          User::WallpaperType type,
                          const UserImage& wallpaper);

  // Sets wallpaper to default wallpaper.
  void SetDefaultWallpaper();

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

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution.
  void UpdateWallpaper();

 private:
  friend class WallpaperManagerBrowserTest;
  typedef std::map<std::string, gfx::ImageSkia> CustomWallpaperMap;

  virtual ~WallpaperManager();

  // The number of wallpapers have loaded. For test only.
  int loaded_wallpapers() const { return loaded_wallpapers_; }

  // Change the wallpapers for users who choose DAILY wallpaper type. Updates
  // current wallpaper if it changed. This function should be called at exactly
  // at 0am if chromeos device is on.
  void BatchUpdateWallpaper();

  // Cache some (or all) logged in users' wallpapers to memory at login
  // screen. It should not compete with first wallpaper loading when boot
  // up/initialize login WebUI page.
  // There are two ways the first wallpaper might be loaded:
  // 1. Loaded on boot. Login WebUI waits for it.
  // 2. When flag --disable-boot-animation is passed. Login WebUI is loaded
  // right away and in 500ms after. Wallpaper started to load.
  // For case 2, should_cache_wallpaper_ is used to indicate if we need to
  // cache wallpapers on wallpaper animation finished. The cache operation
  // should be only executed once.
  void CacheUsersWallpapers();

  // Caches |email|'s wallpaper to memory.
  void CacheUserWallpaper(const std::string& email);

  // Clears all obsolete wallpaper prefs from old version wallpaper pickers.
  void ClearObsoleteWallpaperPrefs();

  // Deletes a list of wallpaper files in |file_list|.
  void DeleteWallpaperInList(const std::vector<FilePath>& file_list);

  // Deletes all |email| related custom or converted wallpapers.
  void DeleteUserWallpapers(const std::string& email);

  // Initialize wallpaper of registered device after device policy is trusted.
  // Note that before device is enrolled, it proceeds with untrusted setting.
  void InitializeRegisteredDeviceWallpaper();

  // Loads |email|'s wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper.
  void LoadWallpaper(const std::string& email,
                     const WallpaperInfo& info,
                     bool update_wallpaper);

  // Gets |email|'s custom wallpaper at |wallpaper_path|. Falls back on original
  // custom wallpaper. When |update_wallpaper| is true, sets wallpaper to the
  // loaded wallpaper. Must run on FILE thread.
  void GetCustomWallpaperInternal(const std::string& email,
                                  const WallpaperInfo& info,
                                  const FilePath& wallpaper_path,
                                  bool update_wallpaper);

  // Gets wallpaper information of |email| from Local State or memory. Returns
  // false if wallpaper information is not found.
  bool GetUserWallpaperInfo(const std::string& email, WallpaperInfo* info);

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.
  void OnWallpaperDecoded(const std::string& email,
                          ash::WallpaperLayout layout,
                          bool update_wallpaper,
                          const UserImage& wallpaper);

  // Generates thumbnail of custom wallpaper on FILE thread. If |persistent| is
  // true, saves original custom image and resized images to disk.
  void ProcessCustomWallpaper(const std::string& email,
                              bool persistent,
                              const WallpaperInfo& info,
                              scoped_ptr<gfx::ImageSkia> image,
                              const UserImage::RawImage& raw_image);

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

  // Starts to load wallpaper at |wallpaper_path|. If |wallpaper_path| is the
  // same as |current_wallpaper_path_|, do nothing. Must be called on UI thread.
  void StartLoad(const std::string& email,
                 const WallpaperInfo& info,
                 bool update_wallpaper,
                 const FilePath& wallpaper_path);

  // Overridden from chromeos::PowerManagerObserver.
  virtual void SystemResumed(const base::TimeDelta& sleep_duration) OVERRIDE;

  // Overridden from chromeos::RootPowerManagerObserver.
  // TODO(derat): Remove this once notifications are sent by powerd.
  virtual void OnResume(const base::TimeDelta& sleep_duration) OVERRIDE;

  // Overridden from system::TimezoneSettings::Observer.
  virtual void TimezoneChanged(const icu::TimeZone& timezone) OVERRIDE;

  // Validates |wallpaper path| and loads corresponding wallpaper. If
  // |wallpaper_path| is not valid, appends png extension to it before loading.
  // Old wallpaper names have a png extension name. However all new wallpapers
  // are saved in jpeg format. We have removed file extension to avoid
  // confusion in this CL (https://codereview.chromium.org/10950014).
  // For wallpapers saved before it, we still need to append png extension to
  // file name.
  void ValidateAndLoadWallpaper(const std::string& email,
                                const WallpaperInfo& info,
                                bool update_wallpaper,
                                const FilePath& wallpaper_path);

  // The number of loaded wallpapers.
  int loaded_wallpapers_;

  // Sequence token associated with wallpaper operations.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  // Wallpaper sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The file path of current loaded/loading custom/online wallpaper.
  FilePath current_wallpaper_path_;

  // Loads user wallpaper from its file.
  scoped_refptr<UserImageLoader> wallpaper_loader_;

  // Logged-in user wallpaper information.
  WallpaperInfo current_user_wallpaper_info_;

  // Caches wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap wallpaper_cache_;

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
