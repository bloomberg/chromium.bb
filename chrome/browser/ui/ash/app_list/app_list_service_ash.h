// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"

class AppListControllerDelegateAsh;
template <typename T> struct DefaultSingletonTraits;

// AppListServiceAsh wraps functionality in ChromeLauncherController and the Ash
// Shell for showing and hiding the app list on the Ash desktop.
class AppListServiceAsh : public AppListServiceImpl {
 public:
  static AppListServiceAsh* GetInstance();

 private:
  friend struct DefaultSingletonTraits<AppListServiceAsh>;

  AppListServiceAsh();
  virtual ~AppListServiceAsh();

  // AppListService overrides:
  virtual base::FilePath GetProfilePath(
      const base::FilePath& user_data_dir) OVERRIDE;
  virtual void CreateForProfile(Profile* default_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* default_profile) OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual void EnableAppList(Profile* initial_profile,
                             AppListEnableSource enable_source) OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual Profile* GetCurrentAppListProfile() OVERRIDE;
  virtual AppListControllerDelegate* GetControllerDelegate() OVERRIDE;

  // ApplistServiceImpl overrides:
  virtual void DestroyAppList() OVERRIDE;

  scoped_ptr<AppListControllerDelegateAsh> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceAsh);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_LIST_SERVICE_ASH_H_
