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
class SequencedTaskRunner;
}  // namespace base

namespace color_utils {
struct ColorProfile;
}  // namespace color_utils

namespace user_manager {
class UserImage;
}  // namespace user_manager

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

using LoadedCallback =
    base::Callback<void(std::unique_ptr<user_manager::UserImage>)>;

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
  // TODO(crbug.com/776464): Make these private and remove the static qualifier
  // after |WallpaperManager::LoadWallpaper| and
  // |WallpaperManager::GetDeviceWallpaperDir| are migrated.
  static base::FilePath dir_user_data_path_;
  static base::FilePath dir_chrome_os_wallpapers_path_;
  static base::FilePath dir_chrome_os_custom_wallpapers_path_;

  // Directory names of custom wallpapers.
  static const char kSmallWallpaperSubDir[];
  static const char kLargeWallpaperSubDir[];
  static const char kOriginalWallpaperSubDir[];
  static const char kThumbnailWallpaperSubDir[];

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

  // TODO(crbug.com/776464): Move |DecodeWallpaperIfApplicable| and
  // |ReadAndDecodeWallpaper| to a separate utility file.
  // If |data_is_ready| is true, start decoding the image, which will run
  // |callback| upon completion; if it's false, run |callback| immediately with
  // an empty image.
  // TODO(crbug.com/776464): Mash and some unit tests can't use this decoder
  // because it depends on the Shell instance. Make it work after all the
  // decoding code is moved to //ash.
  static void DecodeWallpaperIfApplicable(LoadedCallback callback,
                                          std::unique_ptr<std::string> data,
                                          bool data_is_ready);

  // Reads image from |file_path| on disk, and calls
  // |DecodeWallpaperIfApplicable| with the result of |ReadFileToString|.
  static void ReadAndDecodeWallpaper(
      LoadedCallback callback,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::FilePath& file_path);

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

  // Binds the mojom::WallpaperController interface request to this object.
  void BindRequest(mojom::WallpaperControllerRequest request);

  // Add/Remove observers.
  void AddObserver(WallpaperControllerObserver* observer);
  void RemoveObserver(WallpaperControllerObserver* observer);

  // Returns the prominent color based on |color_profile|.
  SkColor GetProminentColor(color_utils::ColorProfile color_profile) const;

  // Provides current image on the wallpaper, or empty gfx::ImageSkia if there
  // is no image, e.g. wallpaper is none.
  gfx::ImageSkia GetWallpaper() const;
  uint32_t GetWallpaperOriginalImageId() const;

  wallpaper::WallpaperLayout GetWallpaperLayout() const;

  // Sets the wallpaper and alerts observers of changes.
  void SetWallpaperImage(const gfx::ImageSkia& image,
                         const wallpaper::WallpaperInfo& info);

  // Implementation of |SetDefaultWallpaper|. Sets wallpaper to default if
  // |show_wallpaper| is true. Otherwise just save the defaut wallpaper to
  // cache. |user_type| is the type of the user initiating the wallpaper
  // request; may be different from the active user.
  void SetDefaultWallpaperImpl(const AccountId& account_id,
                               const user_manager::UserType& user_type,
                               bool show_wallpaper,
                               MovableOnDestroyCallbackHolder on_finish);

  // Implementation of |SetCustomizedDefaultWallpaper|. Sets
  // |default_wallpaper_image_| either to |small_wallpaper_image| or
  // |large_wallpaper_image| depending on |GetAppropriateResolution|.
  void SetCustomizedDefaultWallpaperImpl(
      const base::FilePath& customized_default_wallpaper_file_small,
      std::unique_ptr<gfx::ImageSkia> small_wallpaper_image,
      const base::FilePath& customized_default_wallpaper_file_large,
      std::unique_ptr<gfx::ImageSkia> large_wallpaper_image);

  // Creates an empty wallpaper. Some tests require a wallpaper widget is ready
  // when running. However, the wallpaper widgets are now created
  // asynchronously. If loading a real wallpaper, there are cases that these
  // tests crash because the required widget is not ready. This function
  // synchronously creates an empty widget for those tests to prevent
  // crashes. An example test is SystemGestureEventFilterTest.ThreeFingerSwipe.
  void CreateEmptyWallpaper();

  // Returns custom wallpaper directory by appending corresponding |sub_dir|.
  base::FilePath GetCustomWallpaperDir(const std::string& sub_dir);

  // Prepares wallpaper to lock screen transition. Will apply blur if
  // |locking| is true and remove it otherwise.
  void PrepareWallpaperForLockScreenChange(bool locking);

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnLocalStatePrefServiceInitialized(PrefService* pref_service) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // Returns true if the specified wallpaper is already stored
  // in |current_wallpaper_|.
  // If |compare_layouts| is false, layout is ignored.
  bool WallpaperIsAlreadyLoaded(const gfx::ImageSkia& image,
                                bool compare_layouts,
                                wallpaper::WallpaperLayout layout) const;

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

  // TODO(crbug.com/776464): Make this private. WallpaperInfo should be an
  // internal concept. In addition, change |is_persistent| to |is_ephemeral| to
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
                            bool is_persistent);

  // Initializes wallpaper info for the user to default and saves it to local
  // state if |is_persistent| is true.
  void InitializeUserWallpaperInfo(const AccountId& account_id,
                                   bool is_persistent);

  // Gets encoded wallpaper from cache. Returns true if success.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // Gets path of encoded wallpaper from cache. Returns true if success.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // TODO(crbug.com/776464): Remove this after WallpaperManager is removed.
  // Returns the pointer of |current_user_wallpaper_info_|.
  wallpaper::WallpaperInfo* GetCurrentUserWallpaperInfo();

  // TODO(crbug.com/776464): Remove this after WallpaperManager is removed.
  // Returns the pointer of |wallpaper_cache_map_|.
  CustomWallpaperMap* GetWallpaperCacheMap();

  int num_decode_request_for_testing() {
    return num_decode_request_for_testing_;
  }

  // mojom::WallpaperController overrides:
  void SetClientAndPaths(
      mojom::WallpaperControllerClientPtr client,
      const base::FilePath& user_data_path,
      const base::FilePath& chromeos_wallpapers_path,
      const base::FilePath& chromeos_custom_wallpapers_path) override;
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

  // Creates a WallpaperWidgetController for |root_window|.
  void InstallDesktopController(aura::Window* root_window);

  // Creates a WallpaperWidgetController for all root windows.
  void InstallDesktopControllerForAllWindows();

  // Moves the wallpaper to the specified container across all root windows.
  // Returns true if a wallpaper moved.
  bool ReparentWallpaper(int container);

  // Returns the wallpaper container id for unlocked and locked states.
  int GetWallpaperContainerId(bool locked);

  // Reload the wallpaper. |clear_cache| specifies whether to clear the
  // wallpaper cache or not.
  void UpdateWallpaper(bool clear_cache);

  // Removes |account_id|'s wallpaper info and color cache if it exists.
  void RemoveUserWallpaperInfo(const AccountId& account_id, bool is_persistent);

  // Implementation of |RemoveUserWallpaper|, which deletes |account_id|'s
  // custom wallpapers and directories.
  void RemoveUserWallpaperImpl(const AccountId& account_id,
                               const std::string& wallpaper_files_id);

  // Sets default wallpaper to be the decoded image, and shows the wallpaper now
  // if |show_wallpaper| is true.
  void OnDefaultWallpaperDecoded(
      const base::FilePath& path,
      wallpaper::WallpaperLayout layout,
      bool show_wallpaper,
      MovableOnDestroyCallbackHolder on_finish,
      std::unique_ptr<user_manager::UserImage> user_image);

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

  // Cached current user wallpaper info. Note its location is used as a key for
  // storing |prominent_colors_| in the wallpaper::kWallpaperColors pref. An
  // empty string disables color caching.
  wallpaper::WallpaperInfo current_user_wallpaper_info_;

  // Cached wallpapers of users.
  CustomWallpaperMap wallpaper_cache_map_;

  // Current decoded default image is stored in cache.
  std::unique_ptr<user_manager::UserImage> default_wallpaper_image_;

  // The paths of the customized default wallpapers, if they exist.
  base::FilePath customized_default_wallpaper_small_;
  base::FilePath customized_default_wallpaper_large_;

  gfx::Size current_max_display_size_;

  base::OneShotTimer timer_;

  int wallpaper_reload_delay_;

  bool is_wallpaper_blurred_ = false;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  ScopedSessionObserver scoped_session_observer_;

  std::unique_ptr<ui::CompositorLock> compositor_lock_;

  // Tracks how many wallpapers have been set.
  int wallpaper_count_for_testing_ = 0;

  // Tracks how many wallpaper decoding requests have been initiated.
  int num_decode_request_for_testing_ = 0;

  // Caches the file path of the current decoded wallpaper.
  base::FilePath wallpaper_file_path_for_testing_;

  base::WeakPtrFactory<WallpaperController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperController);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONTROLLER_H_
