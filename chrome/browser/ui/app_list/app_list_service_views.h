// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/app_list_shower_views.h"
#include "ui/app_list/app_list_model.h"

class AppListControllerDelegate;

// AppListServiceViews manages a desktop app list that uses toolkit-views.
class AppListServiceViews : public AppListServiceImpl,
                            public AppListShowerDelegate {
 public:
  explicit AppListServiceViews(
      scoped_ptr<AppListControllerDelegate> controller_delegate);
  ~AppListServiceViews() override;

  // Set |can_dismiss| to prevent the app list dismissing when losing focus. For
  // example, while showing a window-modal dialog.
  void set_can_dismiss(bool can_dismiss) { can_dismiss_ = can_dismiss; }

  AppListShower& shower() { return shower_; }

  // Called by the AppListControllerDelegate when it is told that the app list
  // view must be destroyed.
  virtual void OnViewBeingDestroyed();

  // AppListService overrides:
  void Init(Profile* initial_profile) override;
  void ShowForProfile(Profile* requested_profile) override;
  void ShowForAppInstall(Profile* profile,
                         const std::string& extension_id,
                         bool start_discovery_tracking) override;
  void ShowForCustomLauncherPage(Profile* profile) override;
  void HideCustomLauncherPage() override;
  void DismissAppList() override;
  bool IsAppListVisible() const override;
  gfx::NativeWindow GetAppListWindow() override;
  Profile* GetCurrentAppListProfile() override;
  AppListControllerDelegate* GetControllerDelegate() override;

  // AppListServiceImpl overrides:
  void CreateForProfile(Profile* requested_profile) override;
  void DestroyAppList() override;

  // AppListShowerDelegate overrides:
  AppListViewDelegate* GetViewDelegateForCreate() override;

 private:
  // Switches to |state|, unless it is |INVALID_STATE| (in which case, opens on
  // the default state).
  void ShowForProfileInternal(Profile* profile,
                              app_list::AppListModel::State state);

  // Responsible for creating the app list and responding to profile changes.
  AppListShower shower_;

  bool can_dismiss_;
  scoped_ptr<AppListControllerDelegate> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceViews);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_
