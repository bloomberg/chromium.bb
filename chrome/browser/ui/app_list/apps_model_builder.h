// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/api/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_model.h"

class AppListController;
class ExtensionAppItem;
class Profile;

class AppsModelBuilder : public content::NotificationObserver {
 public:
  AppsModelBuilder(Profile* profile,
                   app_list::AppListModel::Apps* model,
                   AppListController* controller);
  virtual ~AppsModelBuilder();

  // Populates the model.
  void Build();

 private:
  // Populates the model with apps.
  void PopulateApps();

  // Re-sort apps in case app ordinal prefs are changed.
  void ResortApps();

  // Inserts an app based on app ordinal prefs.
  void InsertApp(ExtensionAppItem* app);

  // Returns the index of the application app with |app_id| in |model_|. If
  // no match is found, returns -1.
  int FindApp(const std::string& app_id);

  // Sets the application app with |highlight_app_id_| in |model_| as
  // highlighted. If such an app is found, reset |highlight_app_id_| so that it
  // is highlighted once per install notification.
  void HighlightApp();

  // Returns app instance at given |index|.
  ExtensionAppItem* GetAppAt(size_t index);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  AppListController* controller_;

  // Sub apps model of AppListModel that represents apps grid view.
  app_list::AppListModel::Apps* model_;

  std::string highlight_app_id_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppsModelBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_APPS_MODEL_BUILDER_H_
