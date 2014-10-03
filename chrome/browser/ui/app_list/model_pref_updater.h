// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_MODEL_PREF_UPDATER_H_
#define CHROME_BROWSER_UI_APP_LIST_MODEL_PREF_UPDATER_H_

#include "base/macros.h"
#include "ui/app_list/app_list_model_observer.h"

namespace app_list {

class AppListModel;
class AppListPrefs;

// A class which listens to an AppListModel and updates the local pref model of
// the app list accordingly.
class ModelPrefUpdater : public AppListModelObserver {
 public:
  ModelPrefUpdater(AppListPrefs* app_list_prefs, AppListModel* model);
  virtual ~ModelPrefUpdater();

  // Overridden from AppListModelObserver:
  virtual void OnAppListItemAdded(AppListItem* item) OVERRIDE;
  virtual void OnAppListItemWillBeDeleted(AppListItem* item) OVERRIDE;
  virtual void OnAppListItemUpdated(AppListItem* item) OVERRIDE;

 private:
  void UpdatePrefsFromAppListItem(AppListItem* item);

  AppListPrefs* app_list_prefs_;
  AppListModel* model_;

  DISALLOW_COPY_AND_ASSIGN(ModelPrefUpdater);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_MODEL_PREF_UPDATER_H_
