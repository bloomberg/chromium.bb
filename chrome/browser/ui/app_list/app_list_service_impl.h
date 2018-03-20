// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "ash/public/interfaces/app_list.mojom.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_impl.h"
#include "chrome/browser/ui/app_list/app_list_service.h"

class AppListClientImpl;
class AppListControllerDelegateImpl;
class AppListViewDelegate;

namespace app_list {
class SearchModel;
}  // namespace app_list

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace test {
class AppListServiceImplTestApi;
}

// An implementation of AppListService.
class AppListServiceImpl : public AppListService {
 public:
  ~AppListServiceImpl() override;

  static AppListServiceImpl* GetInstance();

  // Constructor used for testing.
  AppListServiceImpl(const base::CommandLine& command_line,
                     PrefService* local_state);

  // Lazily create the Chrome AppListViewDelegate and set it to the current user
  // profile.
  AppListViewDelegate* GetViewDelegate();

  void RecordAppListLaunch();
  static void RecordAppListAppLaunch();

  // AppListService overrides:
  Profile* GetCurrentAppListProfile() override;
  void Show() override;
  void DismissAppList() override;
  bool IsAppListVisible() const override;
  bool GetTargetVisibility() const override;
  void FlushForTesting() override;
  AppListControllerDelegate* GetControllerDelegate() override;

  // Shows the app list if it isn't already showing and Switches to |state|,
  // unless it is |INVALID_STATE| (in which case, opens on the default state).
  void ShowAndSwitchToState(ash::AppListState state);

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

  // Returns a pointer to control the app list views in ash.
  ash::mojom::AppListController* GetAppListController();

  // TODO(hejq): Search model migration is not done yet. Chrome still accesses
  //             it directly in non-mus+ash mode.
  app_list::SearchModel* GetSearchModelFromAsh();

 protected:
  AppListServiceImpl();

  // Perform startup checks shared between desktop implementations of the app
  // list. Currently this checks command line flags to enable or disable the app
  // list, and records UMA stats delayed from a previous Chrome process.
  void PerformStartupChecks();

 private:
  friend class test::AppListServiceImplTestApi;
  friend struct base::DefaultSingletonTraits<AppListServiceImpl>;
  static void SendAppListStats();

  std::string GetProfileName();

  base::CommandLine command_line_;
  PrefService* local_state_;
  std::unique_ptr<AppListViewDelegate> view_delegate_;

  AppListControllerDelegateImpl controller_delegate_;
  ash::mojom::AppListController* app_list_controller_ = nullptr;
  AppListClientImpl* app_list_client_ = nullptr;

  bool app_list_visible_ = false;
  bool app_list_target_visible_ = false;

  base::WeakPtrFactory<AppListServiceImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceImpl);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_IMPL_H_
