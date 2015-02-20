// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "ui/app_list/app_list_model.h"

class AppListControllerDelegateAsh;
template <typename T> struct DefaultSingletonTraits;

// AppListServiceAsh wraps functionality in ChromeLauncherController and the Ash
// Shell for showing and hiding the app list on the Ash desktop.
class AppListServiceAsh : public AppListServiceImpl {
 public:
  static AppListServiceAsh* GetInstance();

  // AppListService overrides:
  void Init(Profile* initial_profile) override;

 private:
  friend struct DefaultSingletonTraits<AppListServiceAsh>;

  AppListServiceAsh();
  ~AppListServiceAsh() override;

  // Shows the app list if it isn't already showing and Switches to |state|,
  // unless it is |INVALID_STATE| (in which case, opens on the default state).
  void ShowAndSwitchToState(app_list::AppListModel::State state);

  // AppListService overrides:
  base::FilePath GetProfilePath(const base::FilePath& user_data_dir) override;
  void ShowForProfile(Profile* default_profile) override;
  void ShowForAppInstall(Profile* profile,
                         const std::string& extension_id,
                         bool start_discovery_tracking) override;
  void ShowForCustomLauncherPage(Profile* profile) override;
  bool IsAppListVisible() const override;
  void DismissAppList() override;
  void EnableAppList(Profile* initial_profile,
                     AppListEnableSource enable_source) override;
  gfx::NativeWindow GetAppListWindow() override;
  Profile* GetCurrentAppListProfile() override;
  AppListControllerDelegate* GetControllerDelegate() override;

  // ApplistServiceImpl overrides:
  void CreateForProfile(Profile* default_profile) override;
  void DestroyAppList() override;

  scoped_ptr<AppListControllerDelegateAsh> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
