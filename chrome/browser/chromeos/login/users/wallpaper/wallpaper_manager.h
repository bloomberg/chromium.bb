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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class CommandLine;
class SequencedTaskRunner;
}

namespace user_manager {
class User;
class UserImage;
}

namespace chromeos {

struct WallpaperInfo {
  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user id hash) or online wallpaper URL.
  std::string location;
  ash::WallpaperLayout layout;
  user_manager::User::WallpaperType type;
  base::Time date;
  bool operator==(const WallpaperInfo& other) {
    return (location == other.location) && (layout == other.layout) &&
        (type == other.type);
  }
};

class MovableOnDestroyCallback;
typedef scoped_ptr<MovableOnDestroyCallback> MovableOnDestroyCallbackHolder;

class WallpaperManagerBrowserTest;

// Name of wallpaper sequence token.
extern const char kWallpaperSequenceTokenName[];

// File path suffices of resized small or large wallpaper.
// TODO(bshe): Use the same sub folder system as custom wallpapers use.
// crbug.com/174928
extern const char kSmallWallpaperSuffix[];
extern const char kLargeWallpaperSuffix[];

// Directory names of custom wallpapers.
extern const char kSmallWallpaperSubDir[];
extern const char kLargeWallpaperSubDir[];
extern const char kOriginalWallpaperSubDir[];
extern const char kThumbnailWallpaperSubDir[];

// The width and height of small/large resolution wallpaper. When screen size is
// smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|, the
// small resolution wallpaper should be used. Otherwise, use the large
// resolution wallpaper.
extern const int kSmallWallpaperMaxWidth;
extern const int kSmallWallpaperMaxHeight;
extern const int kLargeWallpaperMaxWidth;
extern const int kLargeWallpaperMaxHeight;

// The width and height of wallpaper thumbnails.
extern const int kWallpaperThumbnailWidth;
extern const int kWallpaperThumbnailHeight;

// This singleton class maintains wallpapers for users who have logged into this
// Chrome OS device.
class WallpaperManager: public content::NotificationObserver {
 public:
  enum WallpaperResolution {
    WALLPAPER_RESOLUTION_LARGE,
    WALLPAPER_RESOLUTION_SMALL
  };

  // For testing.
  class TestApi {
   public:
    explicit TestApi(WallpaperManager* wallpaper_manager);
    virtual ~TestApi();

    base::FilePath current_wallpaper_path();

    bool GetWallpaperFromCache(const std::string& user_id,
                               gfx::ImageSkia* image);

    void SetWallpaperCache(const std::string& user_id,
                           const gfx::ImageSkia& image);

    void ClearDisposableWallpaperCache();

   private:
    WallpaperManager* wallpaper_manager_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  // This should be public to allow access from functions in anonymous
  // namespace.
  class CustomizedWallpaperRescaledFiles;

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWallpaperAnimationFinished(const std::string& user_id) = 0;
    virtual void OnUpdateWallpaperForTesting() {}
    virtual void OnPendingListEmptyForTesting() {}
  };

  // This is "wallpaper either scheduled to load, or loading right now".
  //
  // While enqueued, it defines moment in the future, when it will be loaded.
  // Enqueued but not started request might be updated by subsequent load
  // request. Therefore it's created empty, and updated being enqueued.
  //
  // PendingWallpaper is owned by WallpaperManager, but reference to this object
  // is passed to other threads by PostTask() calls, therefore it is
  // RefCountedThreadSafe.
  class PendingWallpaper : public base::RefCountedThreadSafe<PendingWallpaper> {
   public:
    // Do LoadWallpaper() - image not found in cache.
    PendingWallpaper(const base::TimeDelta delay, const std::string& user_id);

    // There are 4 cases in SetUserWallpaper:
    // 1) gfx::ImageSkia is found in cache.
    //    - Schedule task to (probably) resize it and install:
    //    call ash::Shell::GetInstance()->desktop_background_controller()->
    //          SetCustomWallpaper(user_wallpaper, layout);
    // 2) WallpaperInfo is found in cache
    //    - need to LoadWallpaper(), resize and install.
    // 3) wallpaper path is not NULL, load image URL, then resize, etc...
    // 4) SetDefaultWallpaper (either on some error, or when user is new).
    void ResetSetWallpaperImage(const gfx::ImageSkia& image,
                                const WallpaperInfo& info);
    void ResetLoadWallpaper(const WallpaperInfo& info);
    void ResetSetCustomWallpaper(const WallpaperInfo& info,
                                 const base::FilePath& wallpaper_path);
    void ResetSetDefaultWallpaper();

   private:
    friend class base::RefCountedThreadSafe<PendingWallpaper>;

    ~PendingWallpaper();

    // All Reset*() methods use SetMode() to set object to new state.
    void SetMode(const gfx::ImageSkia& image,
                 const WallpaperInfo& info,
                 const base::FilePath& wallpaper_path,
                 const bool is_default);

    // This method is usually triggered by timer to actually load request.
    void ProcessRequest();

    // This method is called by callback, when load request is finished.
    void OnWallpaperSet();

    std::string user_id_;
    WallpaperInfo info_;
    gfx::ImageSkia user_wallpaper_;
    base::FilePath wallpaper_path_;

    // Load default wallpaper instead of user image.
    bool default_;

    // This is "on destroy" callback that will call OnWallpaperSet() when
    // image will be loaded.
    MovableOnDestroyCallbackHolder on_finish_;
    base::OneShotTimer<WallpaperManager::PendingWallpaper> timer;

    // Load start time to calculate duration.
    base::Time started_load_at_;

    DISALLOW_COPY_AND_ASSIGN(PendingWallpaper);
  };

  WallpaperManager();
  virtual ~WallpaperManager();

  // Get pointer to singleton WallpaperManager instance, create it if necessary.
  static WallpaperManager* Get();

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice. |output_skia| is
  // optional (may be NULL). Returns true on success.
  static bool ResizeImage(const gfx::ImageSkia& image,
                          ash::WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output,
                          gfx::ImageSkia* output_skia);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice and saves the
  // resized wallpaper to |path|. |output_skia| is optional (may be
  // NULL). Returns true on success.
  static bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                     const base::FilePath& path,
                                     ash::WallpaperLayout layout,
                                     int preferred_width,
                                     int preferred_height,
                                     gfx::ImageSkia* output_skia);

  // Returns the appropriate wallpaper resolution for all root windows.
  static WallpaperResolution GetAppropriateResolution();

  // Returns custom wallpaper path. Append |sub_dir|, |user_id_hash| and |file|
  // to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                               const std::string& user_id_hash,
                                               const std::string& file);

  void SetCommandLineForTesting(base::CommandLine* command_line);

  // Indicates imminent shutdown, allowing the WallpaperManager to remove any
  // observers it has registered.
  void Shutdown();

  // Adds PowerManagerClient, TimeZoneSettings and CrosSettings observers.
  void AddObservers();

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

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

  // Removes all |user_id| related wallpaper info and saved wallpapers.
  void RemoveUserWallpaperInfo(const std::string& user_id);

  // Calls SetCustomWallpaper() with |user_id_hash| received from cryptohome.
  void SetCustomWallpaperOnSanitizedUsername(const std::string& user_id,
                                             const gfx::ImageSkia& image,
                                             bool update_wallpaper,
                                             bool cryptohome_success,
                                             const std::string& user_id_hash);

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences. If |update_wallpaper| is false, don't change
  // wallpaper but only update cache.
  void SetCustomWallpaper(const std::string& user_id,
                          const std::string& user_id_hash,
                          const std::string& file,
                          ash::WallpaperLayout layout,
                          user_manager::User::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool update_wallpaper);

  // Use given files as new default wallpaper.
  // Reloads current wallpaper, if old default was loaded.
  // Current value of default_wallpaper_image_ is destroyed.
  // Sets default_wallpaper_image_ either to |small_wallpaper_image| or
  // |large_wallpaper_image| depending on GetAppropriateResolution().
  void SetDefaultWallpaperPath(
      const base::FilePath& customized_default_wallpaper_file_small,
      scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      scoped_ptr<gfx::ImageSkia> large_wallpaper_image);

  // Sets wallpaper to default wallpaper (asynchronously with zero delay).
  void SetDefaultWallpaperNow(const std::string& user_id);

  // Sets wallpaper to default wallpaper (asynchronously with default delay).
  void SetDefaultWallpaperDelayed(const std::string& user_id);

  // Sets selected wallpaper information for |user_id| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperInfo(const std::string& user_id,
                            const WallpaperInfo& info,
                            bool is_persistent);

  // Sets |user_id|'s wallpaper (asynchronously with zero delay).
  void SetUserWallpaperNow(const std::string& user_id);

  // Sets |user_id|'s wallpaper (asynchronously with default delay).
  void SetUserWallpaperDelayed(const std::string& user_id);

  // Sets wallpaper to |image| (asynchronously with zero delay). If
  // |update_wallpaper| is false, skip change wallpaper but only update cache.
  void SetWallpaperFromImageSkia(const std::string& user_id,
                                 const gfx::ImageSkia& image,
                                 ash::WallpaperLayout layout,
                                 bool update_wallpaper);

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution. (asynchronously with zero delay)
  void UpdateWallpaper(bool clear_cache);

  // Adds given observer to the list.
  void AddObserver(Observer* observer);

  // Removes given observer from the list.
  void RemoveObserver(Observer* observer);

  // Returns whether a wallpaper policy is enforced for |user_id|.
  bool IsPolicyControlled(const std::string& user_id) const;

  // Called when a wallpaper policy has been set for |user_id|.  Blocks user
  // from changing the wallpaper.
  void OnPolicySet(const std::string& policy, const std::string& user_id);

  // Called when the wallpaper policy has been cleared for |user_id|.
  void OnPolicyCleared(const std::string& policy, const std::string& user_id);

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  void OnPolicyFetched(const std::string& policy,
                       const std::string& user_id,
                       scoped_ptr<std::string> data);

  // This is called from CustomizationDocument.
  // |resized_directory| is the directory where resized versions are stored and
  // must be writable.
  void SetCustomizedDefaultWallpaper(const GURL& wallpaper_url,
                                     const base::FilePath& downloaded_file,
                                     const base::FilePath& resized_directory);

  // Returns queue size.
  size_t GetPendingListSizeForTesting() const;

 private:
  friend class TestApi;
  friend class PendingWallpaper;
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  typedef std::map<std::string, gfx::ImageSkia> CustomWallpaperMap;


  // Record data for User Metrics Analysis.
  static void RecordUma(user_manager::User::WallpaperType type, int index);

  // Saves original custom wallpaper to |path| (absolute path) on filesystem
  // and starts resizing operation of the custom wallpaper if necessary.
  static void SaveCustomWallpaper(const std::string& user_id_hash,
                                  const base::FilePath& path,
                                  ash::WallpaperLayout layout,
                                  scoped_ptr<gfx::ImageSkia> image);

  // Moves custom wallpapers from |user_id| directory to |user_id_hash|
  // directory.
  static void MoveCustomWallpapersOnWorker(
      const std::string& user_id,
      const std::string& user_id_hash,
      base::WeakPtr<WallpaperManager> weak_ptr);

  // Gets |user_id|'s custom wallpaper at |wallpaper_path|. Falls back on
  // original custom wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper. Must run on wallpaper sequenced worker thread.
  static void GetCustomWallpaperInternal(
      const std::string& user_id,
      const WallpaperInfo& info,
      const base::FilePath& wallpaper_path,
      bool update_wallpaper,
      MovableOnDestroyCallbackHolder on_finish,
      base::WeakPtr<WallpaperManager> weak_ptr);

  // Resize and save customized default wallpaper.
  static void ResizeCustomizedDefaultWallpaper(
      scoped_ptr<gfx::ImageSkia> image,
      const user_manager::UserImage::RawImage& raw_image,
      const CustomizedWallpaperRescaledFiles* rescaled_files,
      bool* success,
      gfx::ImageSkia* small_wallpaper_image,
      gfx::ImageSkia* large_wallpaper_image);

  // Initialize wallpaper for the specified user to default and saves this
  // settings in local state.
  void InitInitialUserWallpaper(const std::string& user_id, bool is_persistent);

  // Set wallpaper to |user_image| controlled by policy.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void SetPolicyControlledWallpaper(const std::string& user_id,
                                    const user_manager::UserImage& user_image);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const std::string& user_id, gfx::ImageSkia* image);

  // The number of wallpapers have loaded. For test only.
  int loaded_wallpapers() const { return loaded_wallpapers_; }

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

  // Caches |user_id|'s wallpaper to memory.
  void CacheUserWallpaper(const std::string& user_id);

  // Clears disposable ONLINE and CUSTOM wallpaper cache. At multi profile
  // world, logged in users' wallpaper cache is not disposable.
  void ClearDisposableWallpaperCache();

  // Clears all obsolete wallpaper prefs from old version wallpaper pickers.
  void ClearObsoleteWallpaperPrefs();

  // Deletes all |user_id| related custom wallpapers and directories.
  void DeleteUserWallpapers(const std::string& user_id,
                            const std::string& path_to_file);

  // Gets the CommandLine representing the current process's command line.
  base::CommandLine* GetCommandLine();

  // Initialize wallpaper of registered device after device policy is trusted.
  // Note that before device is enrolled, it proceeds with untrusted setting.
  void InitializeRegisteredDeviceWallpaper();

  // Loads |user_id|'s wallpaper. When |update_wallpaper| is true, sets
  // wallpaper to the loaded wallpaper.
  void LoadWallpaper(const std::string& user_id,
                     const WallpaperInfo& info,
                     bool update_wallpaper,
                     MovableOnDestroyCallbackHolder on_finish);

  // Called when the original custom wallpaper is moved to the new place.
  // Updates the corresponding user wallpaper info.
  void MoveCustomWallpapersSuccess(const std::string& user_id,
                                   const std::string& user_id_hash);

  // Moves custom wallpaper to a new place. Email address was used as directory
  // name in the old system, this is not safe. New directory system uses
  // user_id_hash instead of user_id. This must be called after user_id_hash is
  // ready.
  void MoveLoggedInUserCustomWallpaper();

  // Gets wallpaper information of |user_id| from Local State or memory. Returns
  // false if wallpaper information is not found.
  bool GetUserWallpaperInfo(const std::string& user_id,
                            WallpaperInfo* info) const;

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void OnWallpaperDecoded(const std::string& user_id,
                          ash::WallpaperLayout layout,
                          bool update_wallpaper,
                          MovableOnDestroyCallbackHolder on_finish,
                          const user_manager::UserImage& user_image);

  // Creates new PendingWallpaper request (or updates currently pending).
  void ScheduleSetUserWallpaper(const std::string& user_id, bool delayed);

  // Sets wallpaper to default.
  void DoSetDefaultWallpaper(
      const std::string& user_id,
      MovableOnDestroyCallbackHolder on_finish);

  // Starts to load wallpaper at |wallpaper_path|. If |wallpaper_path| is the
  // same as |current_wallpaper_path_|, do nothing. Must be called on UI thread.
  void StartLoad(const std::string& user_id,
                 const WallpaperInfo& info,
                 bool update_wallpaper,
                 const base::FilePath& wallpaper_path,
                 MovableOnDestroyCallbackHolder on_finish);

  // After completed load operation, update average load time.
  void SaveLastLoadTime(const base::TimeDelta elapsed);

  // Notify all registered observers.
  void NotifyAnimationFinished();

  // Returns modifiable PendingWallpaper.
  // Returns pending_inactive_ or creates new PendingWallpaper if necessary.
  PendingWallpaper* GetPendingWallpaper(const std::string& user_id,
                                        bool delayed);

  // This is called by PendingWallpaper when load is finished.
  void RemovePendingWallpaperFromList(PendingWallpaper* pending);

  // Calculate delay for next wallpaper load.
  // It is usually average wallpaper load time.
  // If last wallpaper load happened long ago, timeout should be reduced by
  // the time passed after last wallpaper load. So usual user experience results
  // in zero delay.
  base::TimeDelta GetWallpaperLoadDelay() const;

  // This is called after we check that supplied default wallpaper files exist.
  void SetCustomizedDefaultWallpaperAfterCheck(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files);

  // Starts rescaling of customized wallpaper.
  void OnCustomizedDefaultWallpaperDecoded(
      const GURL& wallpaper_url,
      scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      const user_manager::UserImage& user_image);

  // Check the result of ResizeCustomizedDefaultWallpaper and finally
  // apply Customized Default Wallpaper.
  void OnCustomizedDefaultWallpaperResized(
      const GURL& wallpaper_url,
      scoped_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      scoped_ptr<bool> success,
      scoped_ptr<gfx::ImageSkia> small_wallpaper_image,
      scoped_ptr<gfx::ImageSkia> large_wallpaper_image);

  // Init |*default_*_wallpaper_file_| from given command line and
  // clear |default_wallpaper_image_|.
  void SetDefaultWallpaperPathsFromCommandLine(base::CommandLine* command_line);

  // Sets wallpaper to decoded default.
  void OnDefaultWallpaperDecoded(const base::FilePath& path,
                                 const ash::WallpaperLayout layout,
                                 scoped_ptr<user_manager::UserImage>* result,
                                 MovableOnDestroyCallbackHolder on_finish,
                                 const user_manager::UserImage& user_image);

  // Start decoding given default wallpaper.
  void StartLoadAndSetDefaultWallpaper(
      const base::FilePath& path,
      const ash::WallpaperLayout layout,
      MovableOnDestroyCallbackHolder on_finish,
      scoped_ptr<user_manager::UserImage>* result_out);

  // Returns wallpaper subdirectory name for current resolution.
  const char* GetCustomWallpaperSubdirForCurrentResolution();

  // Init default_wallpaper_image_ with 1x1 image of default color.
  void CreateSolidDefaultWallpaper();

  // The number of loaded wallpapers.
  int loaded_wallpapers_;

  // Sequence token associated with wallpaper operations.
  base::SequencedWorkerPool::SequenceToken sequence_token_;

  // Wallpaper sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The file path of current loaded/loading custom/online wallpaper.
  base::FilePath current_wallpaper_path_;

  // Loads user wallpaper from its file.
  scoped_refptr<UserImageLoader> wallpaper_loader_;

  // Logged-in user wallpaper information.
  WallpaperInfo current_user_wallpaper_info_;

  // If non-NULL, used in place of the real command line.
  base::CommandLine* command_line_for_testing_;

  // Caches wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap wallpaper_cache_;

  // The last selected user on user pod row.
  std::string last_selected_user_;

  bool should_cache_wallpaper_;

  scoped_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;

  content::NotificationRegistrar registrar_;

  ObserverList<Observer> observers_;

  // These members are for the scheduler:

  // When last load attempt finished.
  base::Time last_load_finished_at_;

  // last N wallpaper loads times.
  std::deque<base::TimeDelta> last_load_times_;

  // Pointer to last inactive (waiting) entry of 'loading_' list.
  // NULL when there is no inactive request.
  PendingWallpaper* pending_inactive_;

  // Owns PendingWallpaper.
  // PendingWallpaper deletes itself from here on load complete.
  // All pending will be finally deleted on destroy.
  typedef std::vector<scoped_refptr<PendingWallpaper> > PendingList;
  PendingList loading_;

  base::FilePath default_small_wallpaper_file_;
  base::FilePath default_large_wallpaper_file_;

  base::FilePath guest_small_wallpaper_file_;
  base::FilePath guest_large_wallpaper_file_;

  // Current decoded default image is stored in cache.
  scoped_ptr<user_manager::UserImage> default_wallpaper_image_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
