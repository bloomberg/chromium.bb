// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONTROLLER_H_
#define ASH_WALLPAPER_WALLPAPER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/interfaces/wallpaper.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "components/user_manager/user_type.h"
#include "components/wallpaper/wallpaper_color_calculator_observer.h"
#include "components/wallpaper/wallpaper_info.h"
#include "components/wallpaper/wallpaper_resizer_observer.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/gfx/image/image_skia.h"

class PrefRegistrySimple;

namespace base {
class RefCountedBytes;
class SequencedTaskRunner;
}  // namespace base

namespace color_utils {
struct ColorProfile;
}  // namespace color_utils

namespace wallpaper {
class WallpaperColorCalculator;
class WallpaperResizer;
}  // namespace wallpaper

namespace ash {

class WallpaperControllerObserver;

// The |CustomWallpaperElement| contains |first| the path of the image which
// is currently being loaded and or in progress of being loaded and |second|
// the image itself.
using CustomWallpaperElement = std::pair<base::FilePath, gfx::ImageSkia>;
using CustomWallpaperMap = std::map<AccountId, CustomWallpaperElement>;

using LoadedCallback = base::Callback<void(const gfx::ImageSkia& image)>;

// Controls the desktop background wallpaper:
//   - Sets a wallpaper image and layout;
//   - Handles display change (add/remove display, configuration change etc);
//   - Calculates prominent colors.
//   - Move wallpaper to locked container(s) when session state is not ACTIVE to
//     hide the user desktop and move it to unlocked container when session
//     state is ACTIVE;
class ASH_EXPORT WallpaperController
    : public mojom::WallpaperController,
      public WindowTreeHostManager::Observer,
      public ShellObserver,
      public wallpaper::WallpaperResizerObserver,
      public wallpaper::WallpaperColorCalculatorObserver,
      public SessionObserver,
      public ui::CompositorLockClient {
 public:
  enum WallpaperMode { WALLPAPER_NONE, WALLPAPER_IMAGE };

  enum WallpaperResolution {
    WALLPAPER_RESOLUTION_LARGE,
    WALLPAPER_RESOLUTION_SMALL
  };

  // The value assigned if extraction fails or the feature is disabled (e.g.
  // command line, lock/login screens).
  static const SkColor kInvalidColor;

  // The paths of wallpaper directories.
  // TODO(crbug.com/776464): Move these to anonymous namespace after
  // |WallpaperManager::LoadWallpaper| and
  // |WallpaperManager::GetDeviceWallpaperDir| are migrated.
  static base::FilePath dir_user_data_path_;
  static base::FilePath dir_chrome_os_wallpapers_path_;
  static base::FilePath dir_chrome_os_custom_wallpapers_path_;

  // Directory names of custom wallpapers.
  static const char kSmallWallpaperSubDir[];
  static const char kLargeWallpaperSubDir[];
  static const char kOriginalWallpaperSubDir[];
  static const char kThumbnailWallpaperSubDir[];

  // File path suffices of resized small or large wallpaper.
  static const char kSmallWallpaperSuffix[];
  static const char kLargeWallpaperSuffix[];

  // The width and height of small/large resolution wallpaper. When screen size
  // is smaller than |kSmallWallpaperMaxWidth| and |kSmallWallpaperMaxHeight|,
  // the small resolution wallpaper should be used. Otherwise, use the large
  // resolution wallpaper.
  static const int kSmallWallpaperMaxWidth;
  static const int kSmallWallpaperMaxHeight;
  static const int kLargeWallpaperMaxWidth;
  static const int kLargeWallpaperMaxHeight;

  // The width and height of wallpaper thumbnails.
  static const int kWallpaperThumbnailWidth;
  static const int kWallpaperThumbnailHeight;

  // The color of the wallpaper if no other wallpaper images are available.
  static const SkColor kDefaultWallpaperColor;

  WallpaperController();
  ~WallpaperController() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Returns the maximum size of all displays combined in native
  // resolutions.  Note that this isn't the bounds of the display who
  // has maximum resolutions. Instead, this returns the size of the
  // maximum width of all displays, and the maximum height of all displays.
  static gfx::Size GetMaxDisplaySizeInNative();

  // Returns the appropriate wallpaper resolution for all root windows.
  static WallpaperResolution GetAppropriateResolution();

  // Returns wallpaper subdirectory name for current resolution.
  static std::string GetCustomWallpaperSubdirForCurrentResolution();

  // Returns custom wallpaper path. Appends |sub_dir|, |wallpaper_files_id| and
  // |file_name| to custom wallpaper directory.
  static base::FilePath GetCustomWallpaperPath(
      const std::string& sub_dir,
      const std::string& wallpaper_files_id,
      const std::string& file_name);

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  static base::FilePath GetCustomWallpaperDir(const std::string& sub_dir);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice. |output_skia| is
  // optional (may be null). Returns true on success.
  static bool ResizeImage(const gfx::ImageSkia& image,
                          wallpaper::WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output,
                          gfx::ImageSkia* output_skia);

  // Resizes |image| to a resolution which is nearest to |preferred_width| and
  // |preferred_height| while respecting the |layout| choice and saves the
  // resized wallpaper to |path|. |output_skia| is optional (may be
  // null). Returns true on success.
  static bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                                     const base::FilePath& path,
                                     wallpaper::WallpaperLayout layout,
                                     int preferred_width,
                                     int preferred_height,
                                     gfx::ImageSkia* output_skia);

  // TODO(crbug.com/776464): These utility functions for device policy wallpaper
  // are temporary during the refactoring.
  // Returns the file directory where the downloaded device wallpaper is saved.
  static base::FilePath GetDeviceWallpaperDir();

  // Returns the full path for the downloaded device wallpaper.
  static base::FilePath GetDeviceWallpaperFilePath();

  // Gets |account_id|'s custom wallpaper at |wallpaper_path|. Falls back to the
  // original custom wallpaper. When |show_wallpaper| is true, shows the
  // wallpaper immediately. Must run on wallpaper sequenced worker thread.
  static void SetWallpaperFromPath(
      const AccountId& account_id,
      const user_manager::UserType& user_type,
      const wallpaper::WallpaperInfo& info,
      const base::FilePath& wallpaper_path,
      bool show_wallpaper,
      const scoped_refptr<base::SingleThreadTaskRunner>& reply_task_runner,
      base::WeakPtr<WallpaperController> weak_ptr);

  // If |data_is_ready| is true, start decoding the image, which will run
  // |callback| upon completion; if it's false, run |callback| immediately with
  // an empty image.
  // TODO(crbug.com/776464): Mash and some unit tests can't use this decoder
  // because it depends on the Shell instance. Make it work after all the
  // decoding code is moved to //ash.
  static void DecodeWallpaperIfApplicable(LoadedCallback callback,
                                          std::unique_ptr<std::string> data,
                                          bool data_is_ready);

  // Creates a 1x1 solid color image to be used as the backup default wallpaper.
  static gfx::ImageSkia CreateSolidColorWallpaper();

  // TODO(crbug.com/776464): All the static |*ForTesting| functions should be
  // moved to the anonymous namespace of |WallpaperControllerTest|.
  //
  // Creates compressed JPEG image of solid color. Result bytes are written to
  // |output|. Returns true if gfx::JPEGCodec::Encode() succeeds.
  static bool CreateJPEGImageForTesting(int width,
                                        int height,
                                        SkColor color,
                                        std::vector<unsigned char>* output);

  // Writes a JPEG image of the specified size and color to |path|. Returns true
  // on success.
  static bool WriteJPEGFileForTesting(const base::FilePath& path,
                                      int width,
                                      int height,
                                      SkColor color);

  // Returns the expected file path of the device policy wallpaper.
  static base::FilePath GetDevicePolicyWallpaperFilePath();

  // Binds the mojom::WallpaperController interface request to this object.
  void BindRequest(mojom::WallpaperControllerRequest request);

  // Add/Remove observers.
  void AddObserver(WallpaperControllerObserver* observer);
  void RemoveObserver(WallpaperControllerObserver* observer);

  // Returns the prominent color based on |color_profile|.
  SkColor GetProminentColor(color_utils::ColorProfile color_profile) const;

  // Returns current image on the wallpaper, or an empty image if there's no
  // wallpaper.
  gfx::ImageSkia GetWallpaper() const;

  // Returns the original image id of the wallpaper before resizing, or 0 if
  // there's no wallpaper.
  uint32_t GetWallpaperOriginalImageId() const;

  // Returns the layout of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  wallpaper::WallpaperLayout GetWallpaperLayout() const;

  // Returns the type of the current wallpaper, or an invalid value if there's
  // no wallpaper.
  wallpaper::WallpaperType GetWallpaperType() const;

  // Sets the wallpaper and alerts observers of changes.
  void SetWallpaperImage(const gfx::ImageSkia& image,
                         const wallpaper::WallpaperInfo& info);

  // Implementation of |SetDefaultWallpaper|. Sets wallpaper to default if
  // |show_wallpaper| is true. Otherwise just save the defaut wallpaper to
  // cache. |user_type| is the type of the user initiating the wallpaper
  // request; may be different from the active user.
  void SetDefaultWallpaperImpl(const AccountId& account_id,
                               const user_manager::UserType& user_type,
                               bool show_wallpaper);

  // TODO(crbug.com/776464): Convert this to a mojo call to replace
  // |SetCustomizedDefaultWallpaper|. If possible, send the paths together with
  // |SetClientAndPaths|.
  // Sets the paths of customized default wallpaper to be used wherever a
  // default wallpaper is needed. Note: it doesn't change the default wallpaper
  // for guest and child accounts.
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& customized_default_wallpaper_file_small,
      const base::FilePath& customized_default_wallpaper_file_large);

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created
  // asynchronously. If loading a real wallpaper, there are cases that these
  // tests crash because the required widget is not ready. This function
  // synchronously creates an empty widget for those tests to prevent
  // crashes. An example test is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Returns whether a wallpaper policy is enforced for |account_id| (not
  // including device policy).
  bool IsPolicyControlled(const AccountId& account_id,
                          bool is_persistent) const;

  // When kiosk app is running or policy is enforced, setting a user wallpaper
  // is not allowed.
  bool CanSetUserWallpaper(const AccountId& account_id,
                           bool is_persistent) const;

  // Prepares wallpaper to lock screen transition. Will apply blur if
  // |locking| is true and remove it otherwise.
  void PrepareWallpaperForLockScreenChange(bool locking);

  // Returns the location of the active user's wallpaper (either an URL or a
  // file path). Returns an empty string if there's no active user, or the
  // active user has not set a user wallpaper.
  std::string GetActiveUserWallpaperLocation();

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnLocalStatePrefServiceInitialized(PrefService* pref_service) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Returns true if the specified wallpaper is already stored in
  // |current_wallpaper_|. If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                wallpaper::WallpaperLayout layout) const;

  // Reads image from |file_path| on disk, and calls
  // |DecodeWallpaperIfApplicable| with the result of |ReadFileToString|.
  void ReadAndDecodeWallpaper(
      LoadedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::FilePath& file_path);

  void set_wallpaper_reload_delay_for_test(int value) {
    wallpaper_reload_delay_ = value;
  }

  // Opens the set wallpaper page in the browser.
  void OpenSetWallpaperPage();

  // Wallpaper should be dimmed for login, lock, OOBE and add user screens.
  bool ShouldApplyDimming() const;

  // Returns whether blur is enabled for login, lock, OOBE and add user screens.
  // See crbug.com/775591.
  bool IsBlurEnabled() const;

  // Returns whether the current wallpaper is blurred.
  bool IsWallpaperBlurred() const { return is_wallpaper_blurred_; }

  // TODO(crbug.com/776464): Change |is_persistent| to |is_ephemeral| to
  // be consistent with |mojom::WallpaperUserInfo|.
  // Sets wallpaper info for |account_id| and saves it to local state if
  // |is_persistent| is true.
  void SetUserWallpaperInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent);

  // Gets wallpaper info of |account_id| from local state, or memory if
  // |is_persistent| is false. Returns false if wallpaper info is not found.
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            wallpaper::WallpaperInfo* info,
                            bool is_persistent) const;

  // Initializes wallpaper info for the user to default and saves it to local
  // state if |is_persistent| is true.
  void InitializeUserWallpaperInfo(const AccountId& account_id,
                                   bool is_persistent);

  // TODO(crbug.com/776464): This method is a temporary workaround during the
  // refactoring. It should be combined with |SetCustomWallpaper|.
  // The same with |SetCustomWallpaper|, except that |image| wasn't once
  // converted to |SkBitmap|, so that the image id is preserved.
  // ArcWallpaperService needs this to track the wallpaper change.
  void SetArcWallpaper(const AccountId& account_id,
                       const user_manager::UserType user_type,
                       const std::string& wallpaper_files_id,
                       const std::string& file_name,
                       const gfx::ImageSkia& image,
                       wallpaper::WallpaperLayout layout,
                       bool is_ephemeral,
                       bool show_wallpaper);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // mojom::WallpaperController overrides:
  void Init(mojom::WallpaperControllerClientPtr client,
            const base::FilePath& user_data_path,
            const base::FilePath& chromeos_wallpapers_path,
            const base::FilePath& chromeos_custom_wallpapers_path,
            bool is_device_wallpaper_policy_enforced) override;
  void SetCustomWallpaper(mojom::WallpaperUserInfoPtr user_info,
                          const std::string& wallpaper_files_id,
                          const std::string& file_name,
                          wallpaper::WallpaperLayout layout,
                          wallpaper::WallpaperType type,
                          const SkBitmap& image,
                          bool show_wallpaper) override;
  void SetOnlineWallpaper(mojom::WallpaperUserInfoPtr user_info,
                          const SkBitmap& image,
                          const std::string& url,
                          wallpaper::WallpaperLayout layout,
                          bool show_wallpaper) override;
  void SetDefaultWallpaper(mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id,
                           bool show_wallpaper) override;
  void SetCustomizedDefaultWallpaper(
      const GURL& wallpaper_url,
      const base::FilePath& file_path,
      const base::FilePath& resized_directory) override;
  void SetDeviceWallpaperPolicyEnforced(bool enforced) override;
  void UpdateCustomWallpaperLayout(mojom::WallpaperUserInfoPtr user_info,
                                   wallpaper::WallpaperLayout layout) override;
  // TODO(crbug.com/776464): |ShowUserWallpaper| is incomplete until device
  // policy wallpaper is migrated. Callers from chrome should use
  // |WallpaperManager::ShowUserWallpaper| instead. Callers in ash can't use it
  // for now.
  void ShowUserWallpaper(mojom::WallpaperUserInfoPtr user_info) override;
  void ShowSigninWallpaper() override;
  void RemoveUserWallpaper(mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id) override;
  void SetWallpaper(const SkBitmap& wallpaper,
                    const wallpaper::WallpaperInfo& wallpaper_info) override;
  void AddObserver(mojom::WallpaperObserverAssociatedPtrInfo observer) override;
  void GetWallpaperColors(GetWallpaperColorsCallback callback) override;

  // WallpaperResizerObserver:
  void OnWallpaperResized() override;

  // WallpaperColorCalculatorObserver:
  void OnColorCalculationComplete() override;

  // Sets dummy values for wallpaper directories.
  void InitializePathsForTesting();

  // Shows a solid color wallpaper as the substitute for default wallpapers and
  // updates |default_wallpaper_image_|.
  void ShowDefaultWallpaperForTesting();

  // Sets a test client interface with empty file paths.
  void SetClientForTesting(mojom::WallpaperControllerClientPtr client);

  // Flushes the mojo message pipe to chrome.
  void FlushForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest, BasicReparenting);
  FRIEND_TEST_ALL_PREFIXES(WallpaperControllerTest,
                           WallpaperMovementDuringUnlock);
  friend class WallpaperControllerTest;

  // Cached default wallpaper image and file path. The file path can be used to
  // check if the image is outdated (i.e. when there's a new default wallpaper).
  struct CachedDefaultWallpaper {
    gfx::ImageSkia image;
    base::FilePath file_path;
  };

  // Creates a WallpaperWidgetController for |root_window|.
  void InstallDesktopController(aura::Window* root_window);

  // Creates a WallpaperWidgetController for all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves the wallpaper to the specified container across all root windows.
  // Returns true if a wallpaper moved.
  bool ReparentWallpaper(int container);

  // Returns the wallpaper container id for unlocked and locked states.
  int GetWallpaperContainerId(bool locked);

  // Removes |account_id|'s wallpaper info and color cache if it exists.
  void RemoveUserWallpaperInfo(const AccountId& account_id, bool is_persistent);

  // Implementation of |RemoveUserWallpaper|, which deletes |account_id|'s
  // custom wallpapers and directories.
  void RemoveUserWallpaperImpl(const AccountId& account_id,
                               const std::string& wallpaper_files_id);

  // Decodes |account_id|'s wallpaper. Shows the decoded wallpaper if
  // |show_wallpaper| is true.
  void SetWallpaperFromInfo(const AccountId& account_id,
                            const user_manager::UserType& user_type,
                            const wallpaper::WallpaperInfo& info,
                            bool show_wallpaper);

  // Used as the callback of default wallpaper decoding. Sets default wallpaper
  // to be the decoded image, and shows the wallpaper now if |show_wallpaper|
  // is true.
  void OnDefaultWallpaperDecoded(const base::FilePath& path,
                                 wallpaper::WallpaperLayout layout,
                                 bool show_wallpaper,
                                 const gfx::ImageSkia& image);

  // Saves |image| to disk if |user_info->is_ephemeral| is false, or if it is a
  // policy wallpaper for public accounts. Shows the wallpaper immediately if
  // |show_wallpaper| is true, otherwise only sets the wallpaper info and
  // updates the cache.
  void SaveAndSetWallpaper(mojom::WallpaperUserInfoPtr user_info,
                           const std::string& wallpaper_files_id,
                           const std::string& file_name,
                           const gfx::ImageSkia& image,
                           wallpaper::WallpaperType type,
                           wallpaper::WallpaperLayout layout,
                           bool show_wallpaper);

  // A wrapper of |ReadAndDecodeWallpaper| used in |SetWallpaperFromPath|.
  void StartDecodeFromPath(const AccountId& account_id,
                           const user_manager::UserType& user_type,
                           const base::FilePath& wallpaper_path,
                           const wallpaper::WallpaperInfo& info,
                           bool show_wallpaper);

  // Used as the callback of wallpaper decoding. (Wallpapers of type DEFAULT
  // and DEVICE should use their corresponding |*Decoded|, and all other types
  // should use this.) Shows the wallpaper now if |show_wallpaper| is true.
  // Otherwise, only update the cache.
  void OnWallpaperDecoded(const AccountId& account_id,
                          const user_manager::UserType& user_type,
                          const base::FilePath& path,
                          const wallpaper::WallpaperInfo& info,
                          bool show_wallpaper,
                          const gfx::ImageSkia& image);

  // Reloads the current wallpaper. It may change the wallpaper size based on
  // the current display's resolution. If |clear_cache| is true, all wallpaper
  // cache should be cleared. This is required when the display's native
  // resolution changes to a larger resolution (e.g. when hooked up a large
  // external display) and we need to load a larger resolution wallpaper for the
  // display. All the previous small resolution wallpaper cache should be
  // cleared.
  void ReloadWallpaper(bool clear_cache);

  // Sets |prominent_colors_| and notifies the observers if there is a change.
  void SetProminentColors(const std::vector<SkColor>& prominent_colors);

  // Calculates prominent colors based on the wallpaper image and notifies
  // |observers_| of the value, either synchronously or asynchronously. In some
  // cases the wallpaper image will not actually be processed (e.g. user isn't
  // logged in, feature isn't enabled).
  // If an existing calculation is in progress it is destroyed.
  void CalculateWallpaperColors();

  // Returns false when the color extraction algorithm shouldn't be run based on
  // system state (e.g. wallpaper image, SessionState, etc.).
  bool ShouldCalculateColors() const;

  // Move all wallpaper widgets to the locked container.
  // Returns true if the wallpaper moved.
  bool MoveToLockedContainer();

  // Move all wallpaper widgets to unlocked container.
  // Returns true if the wallpaper moved.
  bool MoveToUnlockedContainer();

  // Returns whether the current wallpaper is set by device policy.
  bool IsDevicePolicyWallpaper() const;

  // Returns true if device wallpaper policy is in effect and we are at the
  // login screen right now.
  bool ShouldSetDevicePolicyWallpaper() const;

  // Reads the device wallpaper file and sets it as the current wallpaper. Note
  // when it's called, it's guaranteed that ShouldSetDevicePolicyWallpaper()
  // should be true.
  void SetDevicePolicyWallpaper();

  // Called when the device policy controlled wallpaper has been decoded.
  void OnDevicePolicyWallpaperDecoded(const gfx::ImageSkia& image);

  // When wallpaper resizes, we can check which displays will be affected. For
  // simplicity, we only lock the compositor for the internal display.
  void GetInternalDisplayCompositorLock();

  // CompositorLockClient:
  void CompositorLockTimedOut() override;

  bool locked_;

  WallpaperMode wallpaper_mode_;

  // Client interface in chrome browser.
  mojom::WallpaperControllerClientPtr wallpaper_controller_client_;

  // Bindings for the WallpaperController interface.
  mojo::BindingSet<mojom::WallpaperController> bindings_;

  base::ObserverList<WallpaperControllerObserver> observers_;

  mojo::AssociatedInterfacePtrSet<mojom::WallpaperObserver> mojo_observers_;

  std::unique_ptr<wallpaper::WallpaperResizer> current_wallpaper_;

  // Asynchronous task to extract colors from the wallpaper.
  std::unique_ptr<wallpaper::WallpaperColorCalculator> color_calculator_;

  // The prominent colors extracted from the current wallpaper.
  // kInvalidColor is used by default or if extracting colors fails.
  std::vector<SkColor> prominent_colors_;

  // Caches the color profiles that need to do wallpaper color extracting.
  const std::vector<color_utils::ColorProfile> color_profiles_;

  // The wallpaper info for ephemeral users, which is not stored to local state.
  // See |WallpaperUserInfo::is_ephemeral| for details.
  std::map<AccountId, wallpaper::WallpaperInfo> ephemeral_users_wallpaper_info_;

  // Cached user info of the current user.
  mojom::WallpaperUserInfoPtr current_user_;

  // Cached wallpapers of users.
  CustomWallpaperMap wallpaper_cache_map_;

  // Cached default wallpaper.
  CachedDefaultWallpaper cached_default_wallpaper_;

  // The paths of the customized default wallpapers, if they exist.
  base::FilePath customized_default_wallpaper_small_;
  base::FilePath customized_default_wallpaper_large_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  int wallpaper_reload_delay_;

  bool is_wallpaper_blurred_ = false;

  // Whether the device wallpaper policy is enforced on this device.
  bool is_device_wallpaper_policy_enforced_ = false;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  ScopedSessionObserver scoped_session_observer_;

  std::unique_ptr<ui::CompositorLock> compositor_lock_;

  // Tracks how many wallpapers have been set.
  int wallpaper_count_for_testing_ = 0;

  // The file paths of decoding requests that have been initiated. Must be a
  // list because more than one decoding requests may happen during a single
  // 'set wallpaper' request. (e.g. when a custom wallpaper decoding fails, a
  // default wallpaper decoding is initiated.)
  std::vector<base::FilePath> decode_requests_for_testing_;

  base::WeakPtrFactory<WallpaperController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperController);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_H_
