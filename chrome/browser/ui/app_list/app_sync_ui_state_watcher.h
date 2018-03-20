// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_
#define CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_sync_ui_state_observer.h"

class AppListModelUpdater;
class AppSyncUIState;
class Profile;

// AppSyncUIStateWatcher updates AppListModel status when AppSyncUIState
// of the given profile changes.
class AppSyncUIStateWatcher : public AppSyncUIStateObserver {
 public:
  AppSyncUIStateWatcher(Profile* profile, AppListModelUpdater* model_updater);
  ~AppSyncUIStateWatcher() override;

 private:
  // AppSyncUIStateObserver overrides:
  void OnAppSyncUIStatusChanged() override;

  AppSyncUIState* app_sync_ui_state_;
  // Owned by AppListSyncableService
  AppListModelUpdater* model_updater_;

  DISALLOW_COPY_AND_ASSIGN(AppSyncUIStateWatcher);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_
