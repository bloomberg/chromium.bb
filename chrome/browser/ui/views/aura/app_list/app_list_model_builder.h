// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_MODEL_BUILDER_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "ash/app_list/app_list_model.h"

class Profile;

class AppListModelBuilder {
 public:
  AppListModelBuilder(Profile* profile, ash::AppListModel* model);
  virtual ~AppListModelBuilder();

  // Populates the model.
  void Build();

 private:
  FRIEND_TEST_ALL_PREFIXES(AppListModelBuilderTest, GetExtensionApps);

  void GetExtensionApps();
  void GetBrowserCommands();

  Profile* profile_;
  ash::AppListModel* model_;

  DISALLOW_COPY_AND_ASSIGN(AppListModelBuilder);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_APP_LIST_MODEL_BUILDER_H_
