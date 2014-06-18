// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_service_views.h"

class ActivationTrackerWin;

template <typename T> struct DefaultSingletonTraits;

class AppListServiceWin : public AppListServiceViews {
 public:
  virtual ~AppListServiceWin();

  static AppListServiceWin* GetInstance();

  // AppListService overrides:
  virtual void SetAppListNextPaintCallback(void (*callback)()) OVERRIDE;
  virtual void HandleFirstRun() OVERRIDE;
  virtual void Init(Profile* initial_profile) OVERRIDE;
  virtual void ShowForProfile(Profile* requested_profile) OVERRIDE;
  virtual void CreateShortcut() OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<AppListServiceWin>;

  // AppListServiceViews overrides:
  virtual void OnViewBeingDestroyed();

  // AppListShowerDelegate overrides:
  virtual void OnViewCreated() OVERRIDE;
  virtual void OnViewDismissed() OVERRIDE;
  virtual void MoveNearCursor(app_list::AppListView* view) OVERRIDE;

  AppListServiceWin();

  bool IsWarmupNeeded();
  void ScheduleWarmup();

  // Loads the profile last used with the app list and populates the view from
  // it without showing it so that the next show is faster. Does nothing if the
  // view already exists, or another profile is in the middle of being loaded to
  // be shown.
  void LoadProfileForWarmup();
  void OnLoadProfileForWarmup(Profile* initial_profile);

  bool enable_app_list_on_next_init_;
  scoped_ptr<ActivationTrackerWin> activation_tracker_;

  base::Closure next_paint_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppListServiceWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_LIST_WIN_APP_LIST_SERVICE_WIN_H_
