// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_
#define CHROME_BROWSER_UI_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/ash/app_sync_ui_state_observer.h"

namespace app_list {
class AppListModel;
}

class AppSyncUIState;
class Profile;

// AppSyncUIStateWatcher updates AppListModel status when AppSyncUIState
// of the given profile changes.
class AppSyncUIStateWatcher : public AppSyncUIStateObserver {
 public:
  AppSyncUIStateWatcher(Profile* profile, app_list::AppListModel* model);
  virtual ~AppSyncUIStateWatcher();

 private:
  // AppSyncUIStateObserver overrides:
  virtual void OnAppSyncUIStatusChanged() OVERRIDE;

  AppSyncUIState* app_sync_ui_state_;
  app_list::AppListModel* model_;  // Owned by AppListView

  DISALLOW_COPY_AND_ASSIGN(AppSyncUIStateWatcher);
};

#endif  // CHROME_BROWSER_UI_ASH_APP_LIST_APP_SYNC_UI_STATE_WATCHER_H_
