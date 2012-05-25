// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APPS_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APPS_MODEL_BUILDER_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_model.h"

class Profile;

class AppsModelBuilder : public content::NotificationObserver {
 public:
  AppsModelBuilder(Profile* profile, app_list::AppListModel::Apps* model);
  virtual ~AppsModelBuilder();

  // Populates the model.
  void Build();

 private:
  typedef std::vector<app_list::AppListItemModel*> Apps;

  FRIEND_TEST_ALL_PREFIXES(AppsModelBuilderTest, GetExtensionApps);
  FRIEND_TEST_ALL_PREFIXES(AppsModelBuilderTest, SortAndPopulateModel);
  FRIEND_TEST_ALL_PREFIXES(AppsModelBuilderTest, InsertItemByTitle);

  void SortAndPopulateModel(const Apps& apps);
  void InsertItemByTitle(app_list::AppListItemModel* app);

  void GetExtensionApps(Apps* apps);
  void CreateSpecialApps();

  // Returns the index of the application app with |app_id| in |model_|. If
  // no match is found, returns -1.
  int FindApp(const std::string& app_id);

  // Sets the application app with |highlight_app_id_| in |model_| as
  // highlighted. If such an app is found, reset |highlight_app_id_| so that it
  // is highlighted once per install notification.
  void HighlightApp();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;

  // Sub apps model of AppListModel that represents apps grid view.
  app_list::AppListModel::Apps* model_;

  // Number of special apps in the model. Special apps index should be ranged
  // from [0, special_apps_count_ - 1].
  int special_apps_count_;

  std::string highlight_app_id_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppsModelBuilder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APPS_MODEL_BUILDER_H_
