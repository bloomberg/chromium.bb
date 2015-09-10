// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_views.h"

class ActivationTrackerWin;

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

class AppListServiceWin : public AppListServiceViews {
 public:
  ~AppListServiceWin() override;

  static AppListServiceWin* GetInstance();

  // AppListService overrides:
  void SetAppListNextPaintCallback(void (*callback)()) override;
  void Init(Profile* initial_profile) override;
  void ShowForProfile(Profile* requested_profile) override;
  void CreateShortcut() override;

 private:
  friend struct base::DefaultSingletonTraits<AppListServiceWin>;

  // AppListServiceViews overrides:
  void OnViewBeingDestroyed() override;

  // AppListShowerDelegate overrides:
  void OnViewCreated() override;
  void OnViewDismissed() override;
  void MoveNearCursor(app_list::AppListView* view) override;

  AppListServiceWin();

  bool IsWarmupNeeded();
  void ScheduleWarmup();

  // Loads the profile last used with the app list and populates the view from
  // it without showing it so that the next show is faster. Does nothing if the
  // view already exists, or another profile is in the middle of being loaded to
  // be shown.
  void LoadProfileForWarmup();
  void OnLoadProfileForWarmup(Profile* initial_profile);

  scoped_ptr<ActivationTrackerWin> activation_tracker_;

  base::Closure next_paint_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
