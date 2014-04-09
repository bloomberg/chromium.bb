// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_

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
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_image.h"
#include "chrome/browser/chromeos/login/user_image_loader.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class CommandLine;
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

class MovableOnDestroyCallback;
typedef scoped_ptr<MovableOnDestroyCallback> MovableOnDestroyCallbackHolder;

class WallpaperManagerBrowserTest;
class UserImage;

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

// This class maintains wallpapers for users who have logged into this Chrome
// OS device.
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

  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnWallpaperAnimationFinished(const std::string& user_id) = 0;
    virtual void OnUpdateWallpaperForTesting() {}
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
    void ResetSetWallpaperImage(const gfx::ImageSkia& user_wallpaper,
                                const WallpaperInfo& info);
    void ResetLoadWallpaper(const WallpaperInfo& info);
    void ResetSetCustomWallpaper(const WallpaperInfo& info,
                                 const base::FilePath& wallpaper_path);
    void ResetSetDefaultWallpaper();

   private:
    friend class base::RefCountedThreadSafe<PendingWallpaper>;

    ~PendingWallpaper();

    // All Reset*() methods use SetMode() to set object to new state.
    void SetMode(const gfx::ImageSkia& user_wallpaper,
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

  static WallpaperManager* Get();

  WallpaperManager();
  virtual ~WallpaperManager();

  void SetCommandLineForTesting(base::CommandLine* command_line);

  // Indicates imminent shutdown, allowing the WallpaperManager to remove any
  // observers it has registered.
  void Shutdown();

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Adds PowerManagerClient, TimeZoneSettings and CrosSettings observers.
  void AddObservers();

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

  // Clears disposable ONLINE and CUSTOM wallpaper cache. At multi profile
  // world, logged in users' wallpaper cache is not disposable.
  void ClearDisposableWallpaperCache();

  // Returns custom wallpaper path. Append |sub_dir|, |user_id_hash| and |file|
  // to custom wallpaper directory.
  base::FilePath GetCustomWallpaperPath(const char* sub_dir,
                                        const std::string& user_id_hash,
                                        const std::string& file) const;

  // Returns filepath to save original custom wallpaper for the given user.
  base::FilePath GetOriginalWallpaperPathForUser(const std::string& user_id);

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

  // Resizes |wallpaper| to a resolution which is nearest to |preferred_width|
  // and |preferred_height| while maintaining aspect ratio.
  bool ResizeWallpaper(const UserImage& wallpaper,
                       ash::WallpaperLayout layout,
                       int preferred_width,
                       int preferred_height,
                       scoped_refptr<base::RefCountedBytes>* output,
                       gfx::ImageSkia* output_skia) const;

  // Resizes |wallpaper| to a resolution which is nearest to |preferred_width|
  // and |preferred_height| while maintaining aspect ratio. And saves the
  // resized wallpaper to |path|. |result| is optional (may be NULL).
  // Returns true on success.
  bool ResizeAndSaveWallpaper(const UserImage& wallpaper,
                              const base::FilePath& path,
                              ash::WallpaperLayout layout,
                              int preferred_width,
                              int preferred_height,
                              gfx::ImageSkia* result_out) const;

  // Saves custom wallpaper to file, post task to generate thumbnail and updates
  // local state preferences. If |update_wallpaper| is false, don't change
  // wallpaper but only update cache.
  void SetCustomWallpaper(const std::string& user_id,
                          const std::string& user_id_hash,
                          const std::string& file,
                          ash::WallpaperLayout layout,
                          User::WallpaperType type,
                          const UserImage& wallpaper,
                          bool update_wallpaper);

  // Sets wallpaper to default wallpaper (asynchronously with zero delay).
  void SetDefaultWallpaperNow(const std::string& user_id);

  // Sets wallpaper to default wallpaper (asynchronously with default delay).
  void SetDefaultWallpaperDelayed(const std::string& user_id);

  // Initialize wallpaper for the specified user to default and saves this
  // settings in local state.
  void InitInitialUserWallpaper(const std::string& user_id,
                                bool is_persistent);

  // Sets selected wallpaper information for |user_id| and saves it to Local
  // State if |is_persistent| is true.
  void SetUserWallpaperInfo(const std::string& user_id,
                            const WallpaperInfo& info,
                            bool is_persistent);

  // Sets last selected user on user pod row.
  void SetLastSelectedUser(const std::string& last_selected_user);

  // Sets |user_id|'s wallpaper (asynchronously with zero delay).
  void SetUserWallpaperNow(const std::string& user_id);

  // Sets |user_id|'s wallpaper (asynchronously with default delay).
  void SetUserWallpaperDelayed(const std::string& user_id);

  // Sets wallpaper to |wallpaper| (asynchronously with zero delay). If
  // |update_wallpaper| is false, skip change wallpaper but only update cache.
  void SetWallpaperFromImageSkia(const std::string& user_id,
                                 const gfx::ImageSkia& wallpaper,
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

  // Returns the appropriate wallpaper resolution for all root windows.
  static WallpaperResolution GetAppropriateResolution();

 private:
  friend class TestApi;
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  typedef std::map<std::string, gfx::ImageSkia> CustomWallpaperMap;

  // Set |wallpaper| controlled by policy.
  void SetPolicyControlledWallpaper(const std::string& user_id,
                                    const UserImage& wallpaper);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const std::string& user_id,
                             gfx::ImageSkia* wallpaper);

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

  // Clears all obsolete wallpaper prefs from old version wallpaper pickers.
  void ClearObsoleteWallpaperPrefs();

  // Deletes everything else except |path| in the same directory.
  void DeleteAllExcept(const base::FilePath& path);

  // Deletes a list of wallpaper files in |file_list|.
  void DeleteWallpaperInList(const std::vector<base::FilePath>& file_list);

  // Deletes all |user_id| related custom wallpapers and directories.
  void DeleteUserWallpapers(const std::string& user_id,
                            const std::string& path_to_file);

  // Creates all new custom wallpaper directories for |user_id_hash| if not
  // exist.
  void EnsureCustomWallpaperDirectories(const std::string& user_id_hash);

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

  // Moves custom wallpapers from |user_id| directory to |user_id_hash|
  // directory.
  void MoveCustomWallpapersOnWorker(const std::string& user_id,
                                    const std::string& user_id_hash);

  // Called when the original custom wallpaper is moved to the new place.
  // Updates the corresponding user wallpaper info.
  void MoveCustomWallpapersSuccess(const std::string& user_id,
                                   const std::string& user_id_hash);

  // Moves custom wallpaper to a new place. Email address was used as directory
  // name in the old system, this is not safe. New directory system uses
  // user_id_hash instead of user_id. This must be called after user_id_hash is
  // ready.
  void MoveLoggedInUserCustomWallpaper();

  // Gets |user_id|'s custom wallpaper at |wallpaper_path|. Falls back on
  // original custom wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper. Must run on wallpaper sequenced worker thread.
  void GetCustomWallpaperInternal(const std::string& user_id,
                                  const WallpaperInfo& info,
                                  const base::FilePath& wallpaper_path,
                                  bool update_wallpaper,
                                  MovableOnDestroyCallbackHolder on_finish);

  // Gets wallpaper information of |user_id| from Local State or memory. Returns
  // false if wallpaper information is not found.
  bool GetUserWallpaperInfo(const std::string& user_id,
                            WallpaperInfo* info) const;

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.
  void OnWallpaperDecoded(const std::string& user_id,
                          ash::WallpaperLayout layout,
                          bool update_wallpaper,
                          MovableOnDestroyCallbackHolder on_finish,
                          const UserImage& wallpaper);

  // Generates thumbnail of custom wallpaper on wallpaper sequenced worker
  // thread. If |persistent| is true, saves original custom image and resized
  // images to disk.
  void ProcessCustomWallpaper(const std::string& user_id_hash,
                              bool persistent,
                              const WallpaperInfo& info,
                              scoped_ptr<gfx::ImageSkia> image,
                              const UserImage::RawImage& raw_image);

  // Record data for User Metrics Analysis.
  void RecordUma(User::WallpaperType type, int index) const;

  // Saves original custom wallpaper to |path| (absolute path) on filesystem
  // and starts resizing operation of the custom wallpaper if necessary.
  void SaveCustomWallpaper(const std::string& user_id_hash,
                           const base::FilePath& path,
                           ash::WallpaperLayout layout,
                           const UserImage& wallpaper);

  // Saves wallpaper image raw |data| to |path| (absolute path) in file system.
  // True on success.
  bool SaveWallpaperInternal(const base::FilePath& path,
                             const char* data,
                             int size) const;

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

  // Notify all registed observers.
  void NotifyAnimationFinished();

  // Returns modifiable PendingWallpaper.
  // Returns pending_inactive_ or creates new PendingWallpaper if necessary.
  PendingWallpaper* GetPendingWallpaper(const std::string& user_id,
                                        bool delayed);

  // Calculate delay for next wallpaper load.
  // It is usually average wallpaper load time.
  // If last wallpaper load happened long ago, timeout should be reduced by
  // the time passed after last wallpaper load. So usual user experience results
  // in zero delay.
  base::TimeDelta GetWallpaperLoadDelay() const;

  // Init |*default_*_wallpaper_file_| from given command line and
  // clear |default_wallpaper_image_|.
  void SetDefaultWallpaperPathsFromCommandLine(base::CommandLine* command_line);

  // Sets wallpaper to decoded default.
  void OnDefaultWallpaperDecoded(const base::FilePath& path,
                                 const ash::WallpaperLayout layout,
                                 scoped_ptr<UserImage>* result,
                                 MovableOnDestroyCallbackHolder on_finish,
                                 const UserImage& wallpaper);

  // Start decoding given default wallpaper.
  void StartLoadAndSetDefaultWallpaper(const base::FilePath& path,
                                       const ash::WallpaperLayout layout,
                                       MovableOnDestroyCallbackHolder on_finish,
                                       scoped_ptr<UserImage>* result_out);

  // Returns wallpaper subdirectory name for current resolution.
  const char* GetCustomWallpaperSubdirForCurrentResolution();

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
  scoped_ptr<UserImage> default_wallpaper_image_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WALLPAPER_MANAGER_H_
