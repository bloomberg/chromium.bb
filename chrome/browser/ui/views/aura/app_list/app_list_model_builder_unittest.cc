// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_group_model.h"
#include "ash/app_list/app_list_item_model.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/aura/app_list/app_list_model_builder.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppListModelBuilderTest : public ExtensionServiceTestBase {
 public:
  AppListModelBuilderTest() {}
  virtual ~AppListModelBuilderTest() {}
};

TEST_F(AppListModelBuilderTest, GetExtensionApps) {
  // Load "app_list" extensions test profile. The test profile has 4 extensions:
  // 1 dummy extension, 2 packaged extension apps and 1 hosted extension app.
  FilePath source_install_dir = data_dir_
      .AppendASCII("app_list")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  service_->Init();

  // There should be 4 extensions in the test profile.
  const ExtensionSet* extensions = service_->extensions();
  ASSERT_EQ(static_cast<size_t>(4),  extensions->size());

  scoped_ptr<aura_shell::AppListModel> model(new aura_shell::AppListModel());
  AppListModelBuilder builder(profile_.get(), model.get());
  builder.GetExtensionApps();

  // Expect to have two app groups.
  EXPECT_EQ(2, model->group_count());

  // Two packaged apps are on the first page and hosted app on the 2nd page.
  aura_shell::AppListItemGroupModel* group1 = model->GetGroup(0);
  EXPECT_EQ("Packaged App 1", group1->GetItem(0)->title());
  EXPECT_EQ("Packaged App 2", group1->GetItem(1)->title());

  aura_shell::AppListItemGroupModel* group2 = model->GetGroup(1);
  EXPECT_EQ("Hosted App", group2->GetItem(0)->title());
}
