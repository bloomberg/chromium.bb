// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"

namespace {

scoped_refptr<extensions::Extension> MakeApp(
    const std::string& name,
    const std::string& id,
    extensions::Extension::InitFromValueFlags flags) {
  std::string err;
  base::DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", "0.0");
  value.SetString("app.launch.web_url", "http://google.com");
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::Manifest::INTERNAL, value, flags, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

// Creates next by natural sort ordering application id. Application id has to
// have 32 chars each in range 'a' to 'p' inclusively.
std::string CreateNextAppId(const std::string& app_id) {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
  std::string next_app_id = app_id;
  size_t index = next_app_id.length() - 1;
  while (index > 0 && next_app_id[index] == 'p')
    next_app_id[index--] = 'a';
  DCHECK(next_app_id[index] != 'p');
  next_app_id[index]++;
  DCHECK(crx_file::id_util::IdIsValid(next_app_id));
  return next_app_id;
}

}  // namespace

class AppListSyncableServiceTest : public AppListTestBase {
 public:
  AppListSyncableServiceTest() = default;
  ~AppListSyncableServiceTest() override = default;

  void SetUp() override {
    AppListTestBase::SetUp();

    // Make sure we have a Profile Manager.
    DCHECK(temp_dir_.CreateUniqueTempDir());
    TestingBrowserProcess::GetGlobal()->SetProfileManager(
        new ProfileManagerWithoutInit(temp_dir_.GetPath()));

    extensions::ExtensionSystem* extension_system =
        extensions::ExtensionSystem::Get(profile_.get());
    DCHECK(extension_system);
    app_list_syncable_service_.reset(
        new app_list::AppListSyncableService(profile_.get(), extension_system));
  }

  void TearDown() override { app_list_syncable_service_.reset(); }

  app_list::AppListModel* model() {
    return app_list_syncable_service_->GetModel();
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<app_list::AppListSyncableService> app_list_syncable_service_;

  DISALLOW_COPY_AND_ASSIGN(AppListSyncableServiceTest);
};

TEST_F(AppListSyncableServiceTest, OEMFolderForConflictingPos) {
  // Create a "web store" app.
  const std::string web_store_app_id(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore", web_store_app_id,
              extensions::Extension::WAS_INSTALLED_BY_DEFAULT);
  service_->AddExtension(store.get());

  // Create some app. Note its id should be greater than web store app id in
  // order to move app in case of conflicting pos after web store app.
  const std::string some_app_id = CreateNextAppId(extensions::kWebStoreAppId);
  scoped_refptr<extensions::Extension> some_app =
      MakeApp("some_app", some_app_id,
              extensions ::Extension::WAS_INSTALLED_BY_DEFAULT);
  service_->AddExtension(some_app.get());

  app_list::AppListItem* web_store_item = model()->FindItem(web_store_app_id);
  ASSERT_TRUE(web_store_item);
  app_list::AppListItem* some_app_item = model()->FindItem(some_app_id);
  ASSERT_TRUE(some_app_item);

  // Simulate position conflict.
  model()->SetItemPosition(web_store_item, some_app_item->position());

  // Install an OEM app. It must be placed by default after web store app but in
  // case of app of the same position should be shifted next.
  const std::string oem_app_id = CreateNextAppId(some_app_id);
  scoped_refptr<extensions::Extension> oem_app = MakeApp(
      "oem_app", oem_app_id, extensions::Extension::WAS_INSTALLED_BY_OEM);
  service_->AddExtension(oem_app.get());

  size_t web_store_app_index;
  size_t some_app_index;
  size_t oem_app_index;
  size_t oem_folder_index;
  EXPECT_TRUE(model()->top_level_item_list()->FindItemIndex(
      web_store_app_id, &web_store_app_index));
  EXPECT_TRUE(model()->top_level_item_list()->FindItemIndex(some_app_id,
                                                            &some_app_index));
  // OEM item is not top level element.
  EXPECT_FALSE(model()->top_level_item_list()->FindItemIndex(oem_app_id,
                                                             &oem_app_index));
  // But OEM folder is.
  EXPECT_TRUE(model()->top_level_item_list()->FindItemIndex(
      app_list::AppListSyncableService::kOemFolderId, &oem_folder_index));

  // Ensure right item sequence.
  EXPECT_EQ(some_app_index, web_store_app_index + 1);
  EXPECT_EQ(oem_folder_index, web_store_app_index + 2);
}
