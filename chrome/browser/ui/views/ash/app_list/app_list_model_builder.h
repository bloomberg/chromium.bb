// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/app_list/app_list_model.h"

class Profile;

class AppListModelBuilder : public content::NotificationObserver {
 public:
  explicit AppListModelBuilder(Profile* profile);
  virtual ~AppListModelBuilder();

  void SetModel(app_list::AppListModel* model);

  // Populates the model.
  void Build(const std::string& query);

 private:
  typedef std::vector<app_list::AppListItemModel*> Items;

  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, GetExtensionApps);
  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, SortAndPopulateModel);
  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, InsertItemByTitle);

  void SortAndPopulateModel(const Items& items);
  void InsertItemByTitle(app_list::AppListItemModel* item);

  void GetExtensionApps(const string16& query, Items* items);
  void CreateSpecialItems();

  // Returns the index of the application item with |app_id| in |model_|. If
  // no match is found, returns -1.
  int FindApp(const std::string& app_id);

  // Sets the application item with |highlight_app_id_| in |model_| as
  // highlighted. If such an item is found, reset |highlight_app_id_| so that it
  // is highlighted once per install notification.
  void HighlightApp();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;

  string16 query_;

  // The model used by AppListView. It is passed in via SetModel and owned by
  // AppListView.
  app_list::AppListModel* model_;

  // Number of special items in the model. Special items index should be ranged
  // from [0, special_items_count_ - 1].
  int special_items_count_;

  std::string highlight_app_id_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelBuilder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
