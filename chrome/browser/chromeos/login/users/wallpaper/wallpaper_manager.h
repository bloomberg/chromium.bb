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

#include "ash/wallpaper/wallpaper_controller.h"
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
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_image/user_image.h"
#include "components/wallpaper/wallpaper_export.h"
#include "components/wallpaper/wallpaper_info.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

namespace base {
class CommandLine;
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace user_manager {
class UserImage;
}  // namespace user_manager

namespace chromeos {

// Asserts that the current task is sequenced with any other task that calls
// this.
void AssertCalledOnWallpaperSequence(base::SequencedTaskRunner* task_runner);

// Name of wallpaper sequence token.
extern const char kWallpaperSequenceTokenName[];

class WallpaperManager : public wm::ActivationChangeObserver,
                         public aura::WindowObserver {
 public:
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

  // Initializes wallpaper. If logged in, loads user's wallpaper. If not logged
  // in, uses a solid color wallpaper. If logged in as a stub user, uses an
  // empty wallpaper.
  void InitializeWallpaper();

  // Updates current wallpaper. It may switch the size of wallpaper based on the
  // current display's resolution.
  void UpdateWallpaper(bool clear_cache);

  // Gets wallpaper information of logged in user.
  bool GetLoggedInUserWallpaperInfo(wallpaper::WallpaperInfo* info);

  // TODO(crbug.com/776464): Convert it to mojo call.
  // A wrapper of |WallpaperController::SetCustomizedDefaultWallpaperPaths|.
  void SetCustomizedDefaultWallpaperPaths(
      const base::FilePath& default_small_wallpaper_file,
      const base::FilePath& default_large_wallpaper_file);

  // Adds |this| as an observer to various settings.
  void AddObservers();

  // Loads wallpaper asynchronously if the current wallpaper is not the
  // wallpaper of logged in user.
  void EnsureLoggedInUserWallpaperLoaded();

  // Called when the policy-set wallpaper has been fetched.  Initiates decoding
  // of the JPEG |data| with a callback to SetPolicyControlledWallpaper().
  void OnPolicyFetched(const std::string& policy,
                       const AccountId& account_id,
                       std::unique_ptr<std::string> data);

  // A wrapper of |WallpaperController::IsPolicyControlled|.
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

  // Opens the wallpaper picker window.
  void OpenWallpaperPicker();

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class WallpaperManagerBrowserTest;
  friend class WallpaperManagerBrowserTestDefaultWallpaper;
  friend class WallpaperManagerPolicyTest;

  WallpaperManager();

  // A wrapper of |WallpaperController::SetUserWallpaperInfo|.
  void SetUserWallpaperInfo(const AccountId& account_id,
                            const wallpaper::WallpaperInfo& info,
                            bool is_persistent);

  // A wrapper of |WallpaperController::GetWallpaperFromCache|.
  bool GetWallpaperFromCache(const AccountId& account_id,
                             gfx::ImageSkia* image);

  // A wrapper of |WallpaperController::GetPathFromCache|.
  bool GetPathFromCache(const AccountId& account_id, base::FilePath* path);

  // The number of wallpapers have loaded. For test only.
  int loaded_wallpapers_for_test() const;

  // Gets the CommandLine representing the current process's command line.
  base::CommandLine* GetCommandLine();

  // Initialize wallpaper of registered device after device policy is trusted.
  // Note that before device is enrolled, it proceeds with untrusted setting.
  void InitializeRegisteredDeviceWallpaper();

  // A wrapper of |WallpaperController::GetUserWallpaperInfo|.
  bool GetUserWallpaperInfo(const AccountId& account_id,
                            wallpaper::WallpaperInfo* info) const;

  // Returns true if the device wallpaper should be set for the account.
  // TODO(xdai): Remove this function after migrating ShowUserWallpaper().
  bool ShouldSetDeviceWallpaper(const AccountId& account_id,
                                std::string* url,
                                std::string* hash);

  // A wrapper of |WallpaperController::SetDefaultWallpaperImpl|.
  void SetDefaultWallpaperImpl(const AccountId& account_id,
                               bool show_wallpaper);

  // Notify all registered observers.
  void NotifyAnimationFinished();

  // Record the Wallpaper App that the user is using right now on Chrome OS.
  void RecordWallpaperAppType();

  // Returns wallpaper subdirectory name for current resolution.
  const char* GetCustomWallpaperSubdirForCurrentResolution();

  // Set wallpaper to |user_image| controlled by policy.  (Takes a UserImage
  // because that's the callback interface provided by UserImageLoader.)
  void SetPolicyControlledWallpaper(
      const AccountId& account_id,
      std::unique_ptr<user_manager::UserImage> user_image);

  // Returns the cached logged-in user wallpaper info, or a dummy value under
  // mash.
  wallpaper::WallpaperInfo* GetCachedWallpaperInfo();

  // Returns the wallpaper cache map, or a dummy value under mash.
  ash::CustomWallpaperMap* GetWallpaperCacheMap();

  // Returns the account id of |current_user_|, or an empty value under mash.
  AccountId GetCurrentUserAccountId();

  std::unique_ptr<CrosSettings::ObserverSubscription>
      show_user_name_on_signin_subscription_;

  // The number of loaded wallpapers.
  int loaded_wallpapers_for_test_ = 0;

  base::ThreadChecker thread_checker_;

  // Wallpaper sequenced task runner.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // If non-NULL, used in place of the real command line.
  base::CommandLine* command_line_for_testing_ = nullptr;

  // A placeholder for |current_user_wallpaper_info_| under mash.
  wallpaper::WallpaperInfo dummy_current_user_wallpaper_info_;

  // A placeholder for |wallpaper_cache_map_| under mash.
  ash::CustomWallpaperMap dummy_wallpaper_cache_map_;

  bool should_cache_wallpaper_ = false;

  base::ObserverList<Observer> observers_;

  bool retry_download_if_failed_ = true;

  ScopedObserver<wm::ActivationClient, wm::ActivationChangeObserver>
      activation_client_observer_;
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_;

  base::WeakPtrFactory<WallpaperManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_WALLPAPER_WALLPAPER_MANAGER_H_
