// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/app_list/app_list_prefs.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/browser/ui/app_list/model_pref_updater.h"
#include "components/pref_registry/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/test/app_list_test_model.h"

namespace app_list {
namespace test {

class TestExtensionAppItem : public AppListItem {
 public:
  explicit TestExtensionAppItem(const std::string& id) : AppListItem(id) {}
  ~TestExtensionAppItem() override {}

  const char* GetItemType() const override {
    return ExtensionAppItem::kItemType;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestExtensionAppItem);
};

class ModelPrefUpdaterTest : public testing::Test {
 public:
  ModelPrefUpdaterTest() {}
  ~ModelPrefUpdaterTest() override {}

  void SetUp() override {
    AppListPrefs::RegisterProfilePrefs(pref_service_.registry());
    prefs_.reset(AppListPrefs::Create(&pref_service_));
    model_.reset(new AppListTestModel());
    pref_updater_.reset(new ModelPrefUpdater(prefs_.get(), model_.get()));
  }

  AppListTestModel* model() { return model_.get(); }

  AppListPrefs* prefs() { return prefs_.get(); }

  bool AppListItemMatchesPrefs(AppListItem* item) {
    scoped_ptr<AppListPrefs::AppListInfo> info =
        prefs_->GetAppListInfo(item->id());
    AppListPrefs::AppListInfo::ItemType expected_type =
        AppListPrefs::AppListInfo::ITEM_TYPE_INVALID;
    if (item->GetItemType() == ExtensionAppItem::kItemType)
      expected_type = AppListPrefs::AppListInfo::APP_ITEM;
    else if (item->GetItemType() == AppListFolderItem::kItemType)
      expected_type = AppListPrefs::AppListInfo::FOLDER_ITEM;

    return info && info->position.Equals(item->position()) &&
           info->name == item->name() && info->parent_id == item->folder_id() &&
           info->item_type == expected_type;
  }

 private:
  user_prefs::TestingPrefServiceSyncable pref_service_;
  scoped_ptr<AppListTestModel> model_;
  scoped_ptr<AppListPrefs> prefs_;
  scoped_ptr<ModelPrefUpdater> pref_updater_;

  DISALLOW_COPY_AND_ASSIGN(ModelPrefUpdaterTest);
};

TEST_F(ModelPrefUpdaterTest, ModelChange) {
  AppListPrefs::AppListInfoMap infos;
  prefs()->GetAllAppListInfos(&infos);
  EXPECT_EQ(0u, infos.size());

  AppListFolderItem* folder =
      new AppListFolderItem("folder1", AppListFolderItem::FOLDER_TYPE_NORMAL);
  model()->AddItem(folder);

  prefs()->GetAllAppListInfos(&infos);
  EXPECT_EQ(1u, infos.size());
  EXPECT_TRUE(AppListItemMatchesPrefs(folder));

  AppListItem* item = new TestExtensionAppItem("Item 0");
  model()->AddItem(item);

  prefs()->GetAllAppListInfos(&infos);
  EXPECT_EQ(2u, infos.size());
  EXPECT_TRUE(AppListItemMatchesPrefs(folder));
  EXPECT_TRUE(AppListItemMatchesPrefs(item));

  model()->MoveItemToFolder(item, folder->id());
  EXPECT_EQ(folder->id(), item->folder_id());

  prefs()->GetAllAppListInfos(&infos);
  EXPECT_EQ(2u, infos.size());
  EXPECT_TRUE(AppListItemMatchesPrefs(folder));
  EXPECT_TRUE(AppListItemMatchesPrefs(item));
}

}  // namespace test
}  // namespace app_list
