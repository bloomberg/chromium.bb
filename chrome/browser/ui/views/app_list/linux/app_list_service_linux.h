// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_SERVICE_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_SERVICE_LINUX_H_

#include "chrome/browser/ui/app_list/app_list_service_views.h"
#include "ui/app_list/views/app_list_view_observer.h"

template <typename T> struct DefaultSingletonTraits;

// AppListServiceLinux manages global resources needed for the app list to
// operate, and controls when the app list is opened and closed.
class AppListServiceLinux : public AppListServiceViews,
                            public app_list::AppListViewObserver {
 public:
  virtual ~AppListServiceLinux();

  static AppListServiceLinux* GetInstance();

  // AppListService overrides:
  virtual void CreateShortcut() OVERRIDE;

  // app_list::AppListViewObserver overrides:
  virtual void OnActivationChanged(views::Widget* widget, bool active) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceLinux>;

  // AppListShowerDelegate overrides:
  virtual void OnViewCreated() OVERRIDE;
  virtual void OnViewBeingDestroyed() OVERRIDE;
  virtual void OnViewDismissed() OVERRIDE;
  virtual void MoveNearCursor(app_list::AppListView* view) OVERRIDE;

  AppListServiceLinux();

  DISALLOW_COPY_AND_ASSIGN(AppListServiceLinux);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_LINUX_APP_LIST_SERVICE_LINUX_H_
