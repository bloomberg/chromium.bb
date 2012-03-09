// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/string16.h"
#include "ash/app_list/app_list_model.h"

class Profile;

class AppListModelBuilder {
 public:
  AppListModelBuilder(Profile* profile, ash::AppListModel* model);
  virtual ~AppListModelBuilder();

  // Populates the model.
  void Build(const std::string& query);

 private:
  typedef std::vector<ash::AppListItemModel*> Items;

  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, GetExtensionApps);
  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, SortAndPopulateModel);

  void SortAndPopulateModel(const Items& items);
  void GetExtensionApps(const string16& query, Items* items);
  void GetBrowserCommands(const string16& query, Items* items);

  Profile* profile_;
  ash::AppListModel* model_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelBuilder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_APP_LIST_APP_LIST_MODEL_BUILDER_H_
