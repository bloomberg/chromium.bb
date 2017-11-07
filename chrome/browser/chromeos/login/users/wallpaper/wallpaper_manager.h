// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_

#include <stddef.h>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "ash/public/interfaces/wallpaper.mojom.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/customization/customization_wallpaper_downloader.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/user_manager/user_manager.h"
#include "components/wallpaper/wallpaper_export.h"
#include "components/wallpaper/wallpaper_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

class PrefRegistrySimple;

namespace base {
class CommandLine;
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace user_manager {
class User;
class UserImage;
}  // namespace user_manager

namespace wallpaper {
class WallpaperFilesId;
}  // namespace wallpaper

namespace chromeos {

// Asserts that the current task is sequenced with any other task that calls
// this.
void AssertCalledOnWallpaperSequence(base::SequencedTaskRunner* task_runner);

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

// A dictionary pref that maps usernames to wallpaper info.
extern const char kUsersWallpaperInfo[];

class WallpaperManager : public ash::mojom::WallpaperPicker,
                         public content::NotificationObserver,
                         public user_manager::UserManager::Observer,
                         public wm::ActivationChangeObserver,
                         public aura::WindowObserver {
 public:
  class PendingWallpaper;

  enum WallpaperResolution {
    WALLPAPER_RESOLUTION_LARGE,
    WALLPAPER_RESOLUTION_SMALL
  };

  class CustomizedWallpaperRescaledFiles {
   public:
    CustomizedWallpaperRescaledFiles(const base::FilePath& path_downloaded,
                                     const base::FilePath& path_rescaled_small,
                                     const base::FilePath& path_rescaled_large);
    bool AllSizesExist() const;

    // Closure will hold unretained pointer to this object. So caller must
    // make sure that the closure will be destoyed before this object.
    // Closure must be called on BlockingPool.
    base::Closure CreateCheckerClosure();

    const base::FilePath& path_downloaded() const;
    const base::FilePath& path_rescaled_small() const;
    const base::FilePath& path_rescaled_large() const;

    bool downloaded_exists() const;
    bool rescaled_small_exists() const;
    bool rescaled_large_exists() const;

   private:
    // Must be called on BlockingPool.
    void CheckCustomizedWallpaperFilesExist();

    const base::FilePath path_downloaded_;
    const base::FilePath path_rescaled_small_;
    const base::FilePath path_rescaled_large_;

    bool downloaded_exists_;
    bool rescaled_small_exists_;
    bool rescaled_large_exists_;

    DISALLOW_COPY_AND_ASSIGN(CustomizedWallpaperRescaledFiles);
  };

  // This object is passed between several threads while wallpaper is being
  // loaded. It will notify callback when last reference to it is removed
  // (thus indicating that the last load action has finished).
  class MovableOnDestroyCallback {
   public:
    explicit MovableOnDestroyCallback(const base::Closure& callback);

    ~MovableOnDestroyCallback();

   private:
    base::Closure callback_;

    DISALLOW_COPY_AND_ASSIGN(MovableOnDestroyCallback);
  };

  using MovableOnDestroyCallbackHolder =
      std::unique_ptr<MovableOnDestroyCallback>;

  class TestApi {
   public:
    explicit TestApi(WallpaperManager* wallpaper_manager);
    virtual ~TestApi();

    bool GetWallpaperFromCache(const AccountId& account_id,
                               gfx::ImageSkia* image);

    bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

    void SetWallpaperCache(const AccountId& account_id,
                           const base::FilePath& path,
                           const gfx::ImageSkia& image);

    void ClearDisposableWallpaperCache();

   private:
    WallpaperManager* wallpaper_manager_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(TestApi);
  };

  class Observer {
   public:
    virtual ~Observer() {}
    // Notified when the wallpaper animation finishes.
    virtual void OnWallpaperAnimationFinished(const AccountId& account_id) {}
    // Notified when the wallpaper is updated.
    virtual void OnUpdateWallpaperForTesting() {}
    // Notified when the wallpaper pending list is empty.
    virtual void OnPendingListEmptyForTesting() {}
  };

  ~WallpaperManager() override;

  // Expects there is no instance of WallpaperManager and create one.
  static void Initialize();

  // Gets pointer to singleton WallpaperManager instance.
  static WallpaperManager* Get();

  // Deletes the existing instance of WallpaperManager. Allows the
  // WallpaperManager to remove any observers it has registered.
  static void Shutdown();

  // Set path IDs for used directories
  static void SetPathIds(int dir_user_data_enum,
                         int dir_chromeos_wallpapers_enum,
                         int dir_chromeos_custom_wallpapers_enum);

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  static base::FilePath GetCustomWallpaperDir(const char* sub_dir);

  // Registers wallpaper manager preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice. |output_skia| is
  // optional (may be NULL). Returns true on success.
  static bool ResizeImage(const gfx::ImageSkia& image,
                          wallpaper::WallpaperLayout layout,
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
                                     wallpaper::WallpaperLayout layout,
                                     int preferred_width,
                                     int preferred_height,
                                     gfx::ImageSkia* output_skia);

  // Returns custom wallpaper path. Append |sub_dir|, |wallpaper_files_id| and
  // |file| to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(
      const char* sub_dir,
      const wallpaper::WallpaperFilesId& wallpaper_files_id,
      const std::string& file);

  // Sets wallpaper from policy or from a local file. Saves the custom wallpaper
  // to file, posts task to generate thumbnail and updates local state. If
  // |show_wallpaper| is false, don't show the new wallpaper now but only update
  // cache.
  void SetCustomWallpaper(const AccountId& account_id,
                          const wallpaper::WallpaperFilesId& wallpaper_files_id,
                          const std::string& file,
                          wallpaper::WallpaperLayout layout,
                          wallpaper::WallpaperType type,
                          const gfx::ImageSkia& image,
                          bool show_wallpaper);

  // Sets wallpaper from the wallpaper picker selection. If |show_wallpaper|
  // is false, don't show the new wallpaper now but only update cache.
  void SetOnlineWallpaper(const AccountId& account_id,
                          const gfx::ImageSkia& image,
                          const std::string& url,
                          wallpaper::WallpaperLayout layout,
                          bool show_wallpaper);

  // Sets |account_id|'s wallpaper to be the default wallpaper. Note: different
  // user types may have different default wallpapers. If |show_wallpaper| is
  // false, don't show the default wallpaper now.
  void SetDefaultWallpaper(const AccountId& account_id, bool show_wallpaper);

  // Called from CustomizationDocument. |resized_directory| is the directory
  // where resized versions are stored and it must be writable.
  void SetCustomizedDefaultWallpaper(const GURL& wallpaper_url,
                                     const base::FilePath& downloaded_file,
                                     const base::FilePath& resized_directory);

  // Shows |account_id|'s wallpaper, which is determined in the following order:
  // 1) Use device policy wallpaper if it exists AND we are at the login screen.
  // 2) Use user policy wallpaper if it exists.
  // 3) Use the wallpaper set by the user (either by |SetOnlineWallpaper| or
  //    |SetCustomWallpaper|), if any.
  // 4) Use the default wallpaper of this user.
  void ShowUserWallpaper(const AccountId& account_id);

  // Used by the gaia-signin UI. Signin wallpaper is considered either as the
  // device policy wallpaper or the default wallpaper.
  void ShowSigninWallpaper();

  // Removes all of |account_id|'s saved wallpapers and related info.
  void RemoveUserWallpaper(const AccountId& account_id);

  // TODO(crbug.com/776464): Make this private. WallpaperInfo should be an
  // internal concept.
  // Sets wallpaper info for |account_id| and saves it to local state if
  // |is_persistent| is true.
  void SetUserWallpaperInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent);

  // Initializes wallpaper. If logged in, loads user's wallpaper. If not logged
  // in, uses a solid color wallpaper. If logged in as a stub user, uses an
  // empty wallpaper.
  void InitializeWallpaper();

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution.
  void UpdateWallpaper(bool clear_cache);

  // Returns if the image is in the pending list. |image_id| can be obtained
  // from gfx::ImageSkia by using WallpaperResizer::GetImageId().
  bool IsPendingWallpaper(uint32_t image_id);

  // Returns the appropriate wallpaper resolution for all root windows.
  WallpaperResolution GetAppropriateResolution();

  // Gets wallpaper information of logged in user.
  bool GetLoggedInUserWallpaperInfo(wallpaper::WallpaperInfo* info);

  // Adds |this| as an observer to various settings.
  void AddObservers();

  // Sets command line and initialize |*default_*_wallpaper_file_|.
  void SetCommandLineForTesting(base::CommandLine* command_line);

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  void OnPolicyFetched(const std::string& policy,
                       const AccountId& account_id,
                       std::unique_ptr<std::string> data);

  // Returns queue size.
  size_t GetPendingListSizeForTesting() const;

  // Ruturns files identifier for the |account_id|.
  wallpaper::WallpaperFilesId GetFilesId(const AccountId& account_id) const;

  // Returns whether a wallpaper policy is enforced for |account_id|.
  bool IsPolicyControlled(const AccountId& account_id) const;

  // Called when a wallpaper policy has been set for |account_id|.  Blocks user
  // from changing the wallpaper.
  void OnPolicySet(const std::string& policy, const AccountId& account_id);

  // Called when the wallpaper policy has been cleared for |account_id|.
  void OnPolicyCleared(const std::string& policy, const AccountId& account_id);

  // Adds given observer to the list.
  void AddObserver(Observer* observer);

  // Removes given observer from the list.
  void RemoveObserver(Observer* observer);

  // ash::mojom::WallpaperPicker:
  void Open() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // user_manager::UserManager::Observer:
  void OnChildStatusChanged(const user_manager::User& user) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Shows the default wallpaper based on the user type of |account_id|. For
  // testing purpose only.
  void ShowDefaultWallpaperForTesting(const AccountId& account_id);

 private:
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  WallpaperManager();

  // The |CustomWallpaperElement| contains |first| the path of the image which
  // is currently being loaded and or in progress of being loaded and |second|
  // the image itself.
  typedef std::pair<base::FilePath, gfx::ImageSkia> CustomWallpaperElement;
  typedef std::map<AccountId, CustomWallpaperElement> CustomWallpaperMap;

  // Saves original custom wallpaper to |path| (absolute path) on filesystem
  // and starts resizing operation of the custom wallpaper if necessary.
  static void SaveCustomWallpaper(
      const wallpaper::WallpaperFilesId& wallpaper_files_id,
      const base::FilePath& path,
      wallpaper::WallpaperLayout layout,
      std::unique_ptr<gfx::ImageSkia> image);

  // Moves custom wallpapers from user email directory to
  // |wallpaper_files_id| directory.
  static void MoveCustomWallpapersOnWorker(
      const AccountId& account_id,
      const wallpaper::WallpaperFilesId& wallpaper_files_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      base::WeakPtr<WallpaperManager> weak_ptr);

  // Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back on
  // original custom wallpaper. When |update_wallpaper| is true, sets wallpaper
  // to the loaded wallpaper. Must run on wallpaper sequenced worker thread.
  static void GetCustomWallpaperInternal(
      const AccountId& account_id,
      const wallpaper::WallpaperInfo& info,
      const base::FilePath& wallpaper_path,
      bool update_wallpaper,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      MovableOnDestroyCallbackHolder on_finish,
      base::WeakPtr<WallpaperManager> weak_ptr);

  // Resize and save customized default wallpaper.
  static void ResizeCustomizedDefaultWallpaper(
      std::unique_ptr<gfx::ImageSkia> image,
      const CustomizedWallpaperRescaledFiles* rescaled_files,
      bool* success,
      gfx::ImageSkia* small_wallpaper_image,
      gfx::ImageSkia* large_wallpaper_image);

  // Initialize wallpaper info for the user to default and saves the settings
  // in local state.
  void InitializeUserWallpaperInfo(const AccountId& account_id);

  // If the device is enterprise managed and the device wallpaper policy exists,
  // set the device wallpaper as the login screen wallpaper.
  bool SetDeviceWallpaperIfApplicable(const AccountId& account_id);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // The number of wallpapers have loaded. For test only.
  int loaded_wallpapers_for_test() const;

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

  // Caches |account_id|'s wallpaper to memory.
  void CacheUserWallpaper(const AccountId& account_id);

  // Clears disposable ONLINE and CUSTOM wallpaper cache. At multi profile
  // world, logged in users' wallpaper cache is not disposable.
  void ClearDisposableWallpaperCache();

  // Deletes all |account_id| related custom wallpapers and directories.
  void DeleteUserWallpapers(const AccountId& account_id);

  // Gets the CommandLine representing the current process's command line.
  base::CommandLine* GetCommandLine();

  // Initialize wallpaper of registered device after device policy is trusted.
  // Note that before device is enrolled, it proceeds with untrusted setting.
  void InitializeRegisteredDeviceWallpaper();

  // Loads |account_id|'s wallpaper. When |update_wallpaper| is true, sets
  // wallpaper to the loaded wallpaper.
  void LoadWallpaper(const AccountId& account_id,
                     const wallpaper::WallpaperInfo& info,
                     bool update_wallpaper,
                     MovableOnDestroyCallbackHolder on_finish);

  // Called when the original custom wallpaper is moved to the new place.
  // Updates the corresponding user wallpaper info.
  void MoveCustomWallpapersSuccess(
      const AccountId& account_id,
      const wallpaper::WallpaperFilesId& wallpaper_files_id);

  // Moves custom wallpaper to a new place. Email address was used as directory
  // name in the old system, this is not safe. New directory system uses
  // wallpaper_files_id instead of e-mail. This must be called after
  // wallpaper_files_id is ready.
  void MoveLoggedInUserCustomWallpaper();

  // Gets wallpaper information of |account_id| from Local State or memory.
  // Returns
  // false if wallpaper information is not found.
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            wallpaper::WallpaperInfo* info) const;

  // Returns true if the device wallpaper should be set for the account.
  bool ShouldSetDeviceWallpaper(const AccountId& account_id,
                                std::string* url,
                                std::string* hash);

  // Returns the file directory where the downloaded device wallpaper is saved.
  base::FilePath GetDeviceWallpaperDir();

  // Returns the full path for the downloaded device wallpaper.
  base::FilePath GetDeviceWallpaperFilePath();

  // Sets wallpaper to the decoded wallpaper if |update_wallpaper| is true.
  // Otherwise, cache wallpaper to memory if not logged in.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void OnWallpaperDecoded(const AccountId& account_id,
                          const wallpaper::WallpaperInfo& info,
                          bool update_wallpaper,
                          MovableOnDestroyCallbackHolder on_finish,
                          std::unique_ptr<user_manager::UserImage> user_image);

  // Sets wallpaper to default if |update_wallpaper| is true. Otherwise just
  // load defaut wallpaper to cache.
  void DoSetDefaultWallpaper(const AccountId& account_id,
                             bool update_wallpaper,
                             MovableOnDestroyCallbackHolder on_finish);

  // Starts to load wallpaper at |wallpaper_path|. If |wallpaper_path| is
  // already loaded for that user, do nothing. Must be called on UI thread.
  void StartLoad(const AccountId& account_id,
                 const wallpaper::WallpaperInfo& info,
                 bool update_wallpaper,
                 const base::FilePath& wallpaper_path,
                 MovableOnDestroyCallbackHolder on_finish);

  // After completed load operation, update average load time.
  void SaveLastLoadTime(const base::TimeDelta elapsed);

  // Notify all registered observers.
  void NotifyAnimationFinished();

  // Calculates delay for the next wallpaper load. In most cases it is zero. It
  // starts with the average wallpaper load time, and is reduced by the time
  // passed after the last wallpaper load.
  base::TimeDelta GetWallpaperLoadDelay() const;

  // This is called after we check that supplied default wallpaper files exist.
  void SetCustomizedDefaultWallpaperAfterCheck(
      const GURL& wallpaper_url,
      const base::FilePath& downloaded_file,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files);

  // Starts rescaling of customized wallpaper.
  void OnCustomizedDefaultWallpaperDecoded(
      const GURL& wallpaper_url,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      std::unique_ptr<user_manager::UserImage> user_image);

  // Check the result of ResizeCustomizedDefaultWallpaper and finally
  // apply Customized Default Wallpaper.
  void OnCustomizedDefaultWallpaperResized(
      const GURL& wallpaper_url,
      std::unique_ptr<CustomizedWallpaperRescaledFiles> rescaled_files,
      std::unique_ptr<bool> success,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image);

  // Init |*default_*_wallpaper_file_| from given command line and
  // clear |default_wallpaper_image_|.
  void SetDefaultWallpaperPathsFromCommandLine(base::CommandLine* command_line);

  // Sets wallpaper to decoded default if |update_wallpaper| is true.
  void OnDefaultWallpaperDecoded(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      bool update_wallpaper,
      std::unique_ptr<user_manager::UserImage>* result,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image);

  // Start decoding given default wallpaper and set it as wallpaper if
  // |update_wallpaper| is true.
  void StartLoadAndSetDefaultWallpaper(
      const base::FilePath& path,
      const wallpaper::WallpaperLayout layout,
      bool update_wallpaper,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage>* result_out);

  // Record the Wallpaper App that the user is using right now on Chrome OS.
  void RecordWallpaperAppType();

  // Returns true if wallpaper files id can be returned successfully.
  bool CanGetWallpaperFilesId() const;

  // Returns wallpaper subdirectory name for current resolution.
  const char* GetCustomWallpaperSubdirForCurrentResolution();

  // Init default_wallpaper_image_ with 1x1 image of default color.
  void CreateSolidDefaultWallpaper();

  // Callback function for GetCustomWallpaperInternal().
  void OnCustomWallpaperFileNotFound(const AccountId& account_id,
                                     const base::FilePath& expected_path,
                                     bool update_wallpaper,
                                     MovableOnDestroyCallbackHolder on_finish);

  // Returns modifiable PendingWallpaper. (Either |pending_inactive_| or a new
  // |PendingWallpaper| object.)
  PendingWallpaper* GetPendingWallpaper();

  // This is called by PendingWallpaper when load is finished.
  void RemovePendingWallpaperFromList(
      PendingWallpaper* finished_loading_request);

  // Set wallpaper to |user_image| controlled by policy.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void SetPolicyControlledWallpaper(
      const AccountId& account_id,
      std::unique_ptr<user_manager::UserImage> user_image);

  // This is called when the device wallpaper policy changes.
  void OnDeviceWallpaperPolicyChanged();
  // This is call after checking if the device wallpaper exists.
  void OnDeviceWallpaperExists(const AccountId& account_id,
                               const std::string& url,
                               const std::string& hash,
                               bool exist);
  // This is called after the device wallpaper is download (either successful or
  // failed).
  void OnDeviceWallpaperDownloaded(const AccountId& account_id,
                                   const std::string& hash,
                                   bool success,
                                   const GURL& url);
  // Check if the device wallpaper matches the hash that's provided in the
  // device wallpaper policy setting.
  void OnCheckDeviceWallpaperMatchHash(const AccountId& account_id,
                                       const std::string& url,
                                       const std::string& hash,
                                       bool match);
  // This is called when the device wallpaper is decoded successfully.
  void OnDeviceWallpaperDecoded(
      const AccountId& account_id,
      std::unique_ptr<user_manager::UserImage> user_image);

  // Use given files as new default wallpaper.
  // Reloads current wallpaper, if old default was loaded.
  // Current value of default_wallpaper_image_ is destroyed.
  // Sets default_wallpaper_image_ either to |small_wallpaper_image| or
  // |large_wallpaper_image| depending on GetAppropriateResolution().
  void SetDefaultWallpaperPath(
      const base::FilePath& customized_default_wallpaper_file_small,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image);

  mojo::Binding<ash::mojom::WallpaperPicker> binding_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  std::unique_ptr<CrosSettings::ObserverSubscription>
      device_wallpaper_image_subscription_;
  std::unique_ptr<CustomizationWallpaperDownloader>
      device_wallpaper_downloader_;

  // The number of loaded wallpapers.
  int loaded_wallpapers_for_test_ = 0;

  base::ThreadChecker thread_checker_;

  // Wallpaper sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Logged-in user wallpaper information.
  wallpaper::WallpaperInfo current_user_wallpaper_info_;

  // If non-NULL, used in place of the real command line.
  base::CommandLine* command_line_for_testing_ = nullptr;

  // Caches wallpapers of users. Accessed only on UI thread.
  CustomWallpaperMap wallpaper_cache_;

  // The last selected user on user pod row.
  AccountId last_selected_user_ = EmptyAccountId();

  bool should_cache_wallpaper_ = false;

  base::ObserverList<Observer> observers_;

  // These members are for the scheduler:

  // When last load attempt finished.
  base::Time last_load_finished_at_;

  // last N wallpaper loads times.
  base::circular_deque<base::TimeDelta> last_load_times_;

  base::FilePath default_small_wallpaper_file_;
  base::FilePath default_large_wallpaper_file_;

  base::FilePath guest_small_wallpaper_file_;
  base::FilePath guest_large_wallpaper_file_;

  base::FilePath child_small_wallpaper_file_;
  base::FilePath child_large_wallpaper_file_;

  // Current decoded default image is stored in cache.
  std::unique_ptr<user_manager::UserImage> default_wallpaper_image_;

  bool retry_download_if_failed_ = true;

  // Pointer to last inactive (waiting) entry of 'loading_' list.
  // NULL when there is no inactive request.
  PendingWallpaper* pending_inactive_ = nullptr;

  // Owns PendingWallpaper.
  // PendingWallpaper deletes itself from here on load complete.
  // All pending will be finally deleted on destroy.
  typedef std::vector<PendingWallpaper*> PendingList;
  PendingList loading_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_client_observer_;
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
