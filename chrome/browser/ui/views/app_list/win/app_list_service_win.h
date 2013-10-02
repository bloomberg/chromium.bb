// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_

#include "base/memory/singleton.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"

namespace app_list{
class AppListModel;
}

class AppListShower;

// Exposes methods required by the AppListServiceTestApi on Windows.
// TODO(tapted): Remove testing methods once they can access the implementation
// from the test api.
class AppListServiceWin : public AppListServiceImpl {
 public:
  AppListServiceWin();
  virtual ~AppListServiceWin();

  app_list::AppListModel* GetAppListModelForTesting();

  static AppListServiceWin* GetInstance();
  void set_can_close(bool can_close);
  void OnAppListClosing();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(
      const base::Closure& callback) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual AppListControllerDelegate* CreateControllerDelegate() OVERRIDE;

  // AppListServiceImpl overrides:
  virtual void CreateShortcut() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceWin>;

  bool IsWarmupNeeded();
  void ScheduleWarmup();

  // Loads the profile last used with the app list and populates the view from
  // it without showing it so that the next show is faster. Does nothing if the
  // view already exists, or another profile is in the middle of being loaded to
  // be shown.
  void LoadProfileForWarmup();
  void OnLoadProfileForWarmup(Profile* initial_profile);

  // Responsible for putting views on the screen.
  scoped_ptr<AppListShower> shower_;

  bool enable_app_list_on_next_init_;

  base::WeakPtrFactory<AppListServiceWin> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceWin);
};

namespace chrome {

AppListService* GetAppListServiceWin();

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
