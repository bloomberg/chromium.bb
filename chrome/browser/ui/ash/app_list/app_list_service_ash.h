// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/app_list.mojom.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/ash/app_list/app_list_controller_ash.h"

class AppListClientImpl;

namespace app_list {
class SearchModel;
}  // namespace app_list

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class AppListControllerDelegateAsh;

// AppListServiceAsh wraps functionality in ChromeLauncherController and the Ash
// Shell for showing and hiding the app list on the Ash desktop.
class AppListServiceAsh : public AppListServiceImpl {
 public:
  static AppListServiceAsh* GetInstance();

  // Returns a pointer to control the app list views in ash.
  ash::mojom::AppListController* GetAppListController();

  // TODO(hejq): Search model migration is not done yet. Chrome still accesses
  //             it directly in non-mus+ash mode.
  app_list::SearchModel* GetSearchModelFromAsh();

  // ProfileAttributesStorage::Observer overrides:
  // On ChromeOS this should never happen. On other platforms, there is always a
  // Non-ash AppListService that is responsible for handling this.
  // TODO(calamity): Ash shouldn't observe the ProfileAttributesStorage at all.
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  // AppListService overrides:
  bool IsAppListVisible() const override;
  bool GetTargetVisibility() const override;
  void FlushForTesting() override;

  // Updates app list (target) visibility from AppListClientImpl.
  void set_app_list_visible(bool visible) { app_list_visible_ = visible; }
  void set_app_list_target_visible(bool visible) {
    app_list_target_visible_ = visible;
  }

  // Sets the pointers to the app list controller in Ash, and the app list
  // client in Chrome.
  void SetAppListControllerAndClient(
      ash::mojom::AppListController* app_list_controller,
      AppListClientImpl* app_list_client);

 private:
  friend struct base::DefaultSingletonTraits<AppListServiceAsh>;
  friend class AppListServiceAshTestApi;

  AppListServiceAsh();
  ~AppListServiceAsh() override;

  // Shows the app list if it isn't already showing and Switches to |state|,
  // unless it is |INVALID_STATE| (in which case, opens on the default state).
  void ShowAndSwitchToState(ash::AppListState state);

  // AppListService overrides:
  base::FilePath GetProfilePath(const base::FilePath& user_data_dir) override;
  void ShowForProfile(Profile* default_profile) override;
  void ShowForAppInstall(Profile* profile,
                         const std::string& extension_id,
                         bool start_discovery_tracking) override;
  void DismissAppList() override;
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override;
  Profile* GetCurrentAppListProfile() override;
  AppListControllerDelegate* GetControllerDelegate() override;

  // ApplistServiceImpl overrides:
  void CreateForProfile(Profile* default_profile) override;
  void DestroyAppList() override;

  AppListControllerDelegateAsh controller_delegate_;
  ash::mojom::AppListController* app_list_controller_ = nullptr;
  AppListClientImpl* app_list_client_ = nullptr;

  bool app_list_visible_ = false;
  bool app_list_target_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
