// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_impl.h"
#include "chrome/browser/ui/app_list/app_list_shower_delegate.h"
#include "chrome/browser/ui/app_list/app_list_shower_views.h"

class AppListControllerDelegate;

// AppListServiceViews manages a desktop app list that uses toolkit-views.
class AppListServiceViews : public AppListServiceImpl,
                            public AppListShowerDelegate {
 public:
  explicit AppListServiceViews(
      scoped_ptr<AppListControllerDelegate> controller_delegate);
  virtual ~AppListServiceViews();

  // Set |can_dismiss| to prevent the app list dismissing when losing focus. For
  // example, while showing a window-modal dialog.
  void set_can_dismiss(bool can_dismiss) { can_dismiss_ = can_dismiss; }

  AppListShower& shower() { return shower_; }

  // Called by the AppListControllerDelegate when it is told that the app list
  // view must be destroyed.
  virtual void OnViewBeingDestroyed();

  // AppListService overrides:
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void CreateForProfile(Profile* requested_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void DismissAppList() OVERRIDE;
  virtual bool IsAppListVisible() const OVERRIDE;
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE;
  virtual Profile* GetCurrentAppListProfile() OVERRIDE;
  virtual AppListControllerDelegate* GetControllerDelegate() OVERRIDE;

  // AppListServiceImpl overrides:
  virtual void DestroyAppList() OVERRIDE;

  // AppListShowerDelegate overrides:
  virtual AppListViewDelegate* GetViewDelegateForCreate() OVERRIDE;

 private:
  // Responsible for creating the app list and responding to profile changes.
  AppListShower shower_;

  bool can_dismiss_;
  scoped_ptr<AppListControllerDelegate> controller_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceViews);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_LIST_SERVICE_VIEWS_H_
