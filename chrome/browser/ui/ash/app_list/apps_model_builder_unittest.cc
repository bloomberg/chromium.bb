// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/apps_model_builder.h"

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_model.h"

namespace {

class TestAppListItemModel : public app_list::AppListItemModel {
 public:
  explicit TestAppListItemModel(const std::string& title) {
    SetTitle(title);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppListItemModel);
};

// Get a string of all apps in |model| joined with ','.
std::string GetModelContent(app_list::AppListModel::Apps* model) {
  std::string content;
  for (size_t i = 0; i < model->item_count(); ++i) {
    if (i > 0)
      content += ',';
    content += model->GetItemAt(i)->title();
  }
  return content;
}

}  // namespace

class AppsModelBuilderTest : public ExtensionServiceTestBase {
 public:
  AppsModelBuilderTest() {}
  virtual ~AppsModelBuilderTest() {}
};

TEST_F(AppsModelBuilderTest, GetExtensionApps) {
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

  AppsModelBuilder builder(profile_.get(), NULL);

  AppsModelBuilder::Apps apps;
  builder.GetExtensionApps(&apps);

  // The apps list would have 3 extension apps in the profile.
  EXPECT_EQ(static_cast<size_t>(3), apps.size());
  EXPECT_EQ("Hosted App", apps[0]->title());
  EXPECT_EQ("Packaged App 1", apps[1]->title());
  EXPECT_EQ("Packaged App 2", apps[2]->title());

  STLDeleteElements(&apps);
}

TEST_F(AppsModelBuilderTest, SortAndPopulateModel) {
  const char* kInput[] = {
    "CB", "Ca", "B", "a",
  };
  const char* kExpected = "a,B,Ca,CB";

  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);

  AppsModelBuilder::Apps apps;
  for (size_t i = 0; i < arraysize(kInput); ++i)
    apps.push_back(new TestAppListItemModel(kInput[i]));

  AppsModelBuilder builder(profile_.get(), model.get());
  builder.SortAndPopulateModel(apps);

  EXPECT_EQ(kExpected, GetModelContent(model.get()));
}

TEST_F(AppsModelBuilderTest, InsertItemByTitle) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get());

  const char* kInput[] = {
    "CB", "Ca", "B", "a", "z", "D"
  };
  const char* kExpected = "a,B,Ca,CB,D,z";

  for (size_t i = 0; i < arraysize(kInput); ++i)
    builder.InsertItemByTitle(new TestAppListItemModel(kInput[i]));

  EXPECT_EQ(kExpected, GetModelContent(model.get()));
}

