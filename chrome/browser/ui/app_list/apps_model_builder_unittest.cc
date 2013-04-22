// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/apps_model_builder.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/extension_sorting.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_model.h"

namespace {

const char kHostedAppId[] = "dceacbkfkmllgmjmbhgkpjegnodmildf";
const char kPackagedApp1Id[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";
const char kPackagedApp2Id[] = "jlklkagmeajbjiobondfhiekepofmljl";

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

scoped_refptr<extensions::Extension> MakeApp(const std::string& name,
                                             const std::string& version,
                                             const std::string& url,
                                             const std::string& id) {
  std::string err;
  DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", version);
  value.SetString("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app =
      extensions::Extension::Create(
          base::FilePath(),
          extensions::Manifest::INTERNAL,
          value,
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT,
          id,
          &err);
  EXPECT_EQ(err, "");
  return app;
}

}  // namespace

class AppsModelBuilderTest : public ExtensionServiceTestBase {
 public:
  AppsModelBuilderTest() {}
  virtual ~AppsModelBuilderTest() {}

  virtual void SetUp() OVERRIDE {
    // Load "app_list" extensions test profile.
    // The test profile has 4 extensions:
    // 1 dummy extension, 2 packaged extension apps and 1 hosted extension app.
    base::FilePath source_install_dir = data_dir_
        .AppendASCII("app_list")
        .AppendASCII("Extensions");
    base::FilePath pref_path = source_install_dir
        .DirName()
        .AppendASCII("Preferences");
    InitializeInstalledExtensionService(pref_path, source_install_dir);
    service_->Init();

    // There should be 4 extensions in the test profile.
    const ExtensionSet* extensions = service_->extensions();
    ASSERT_EQ(static_cast<size_t>(4),  extensions->size());
  }
};

TEST_F(AppsModelBuilderTest, Build) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  // The apps list would have 3 extension apps in the profile.
  EXPECT_EQ(std::string("Packaged App 1,Packaged App 2,Hosted App"),
            GetModelContent(model.get()));
}

TEST_F(AppsModelBuilderTest, HideWebStore) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore",
              "0.0",
              "http://google.com",
              std::string(extension_misc::kWebStoreAppId));
  service_->AddExtension(store);

  // Install an "enterprise web store" app.
  scoped_refptr<extensions::Extension> enterprise_store =
      MakeApp("enterprise_webstore",
              "0.0",
              "http://google.com",
              std::string(extension_misc::kEnterpriseWebStoreAppId));
  service_->AddExtension(enterprise_store);

  // Web stores should be present in the AppListModel.
  app_list::AppListModel::Apps model1;
  AppsModelBuilder builder1(profile_.get(), &model1, NULL);
  builder1.Build();
  std::string content = GetModelContent(&model1);
  EXPECT_NE(std::string::npos, content.find("webstore"));
  EXPECT_NE(std::string::npos, content.find("enterprise_webstore"));

  // Activate the HideWebStoreIcon policy.
  profile_->GetPrefs()->SetBoolean(prefs::kHideWebStoreIcon, true);

  // Web stores should NOT be in the AppListModel.
  app_list::AppListModel::Apps model2;
  AppsModelBuilder builder2(profile_.get(), &model2, NULL);
  builder2.Build();
  content = GetModelContent(&model2);
  EXPECT_EQ(std::string::npos, content.find("webstore"));
  EXPECT_EQ(std::string::npos, content.find("enterprise_webstore"));
}

TEST_F(AppsModelBuilderTest, DisableAndEnable) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ(std::string("Packaged App 1,Packaged App 2,Hosted App"),
            GetModelContent(model.get()));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ(std::string("Packaged App 1,Packaged App 2,Hosted App"),
            GetModelContent(model.get()));
}

TEST_F(AppsModelBuilderTest, Uninstall) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  service_->UninstallExtension(kPackagedApp2Id, false, NULL);
  EXPECT_EQ(std::string("Packaged App 1,Hosted App"),
            GetModelContent(model.get()));

  loop_.RunUntilIdle();
}

TEST_F(AppsModelBuilderTest, OrdinalPrefsChange) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  ExtensionSorting* sorting = service_->extension_prefs()->extension_sorting();

  syncer::StringOrdinal package_app_page =
      sorting->GetPageOrdinal(kPackagedApp1Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page.CreateBefore());
  EXPECT_EQ(std::string("Hosted App,Packaged App 1,Packaged App 2"),
            GetModelContent(model.get()));

  syncer::StringOrdinal app1_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp1Id);
  syncer::StringOrdinal app2_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp2Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page);
  sorting->SetAppLaunchOrdinal(kHostedAppId,
                               app1_ordinal.CreateBetween(app2_ordinal));
  EXPECT_EQ(std::string("Packaged App 1,Hosted App,Packaged App 2"),
            GetModelContent(model.get()));
}

TEST_F(AppsModelBuilderTest, OnExtensionMoved) {
  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  ExtensionSorting* sorting = service_->extension_prefs()->extension_sorting();
  sorting->SetPageOrdinal(kHostedAppId,
                          sorting->GetPageOrdinal(kPackagedApp1Id));

  service_->OnExtensionMoved(kHostedAppId, kPackagedApp1Id, kPackagedApp2Id);
  EXPECT_EQ(std::string("Packaged App 1,Hosted App,Packaged App 2"),
            GetModelContent(model.get()));

  service_->OnExtensionMoved(kHostedAppId, kPackagedApp2Id, std::string());
  EXPECT_EQ(std::string("Packaged App 1,Packaged App 2,Hosted App"),
            GetModelContent(model.get()));

  service_->OnExtensionMoved(kHostedAppId, std::string(), kPackagedApp1Id);
  EXPECT_EQ(std::string("Hosted App,Packaged App 1,Packaged App 2"),
            GetModelContent(model.get()));
}

TEST_F(AppsModelBuilderTest, InvalidOrdinal) {
  // Creates a no-ordinal case.
  ExtensionSorting* sorting = service_->extension_prefs()->extension_sorting();
  sorting->ClearOrdinals(kPackagedApp1Id);

  // Creates an corrupted ordinal case.
  ExtensionScopedPrefs* scoped_prefs = service_->extension_prefs();
  scoped_prefs->UpdateExtensionPref(
      kHostedAppId,
      "page_ordinal",
      base::Value::CreateStringValue("a corrupted ordinal"));

  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);

  // This should not assert or crash.
  builder.Build();
}

TEST_F(AppsModelBuilderTest, OrdinalConfilicts) {
  // Creates conflict ordinals for app1 and app2.
  syncer::StringOrdinal conflict_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();

  ExtensionSorting* sorting = service_->extension_prefs()->extension_sorting();
  sorting->SetPageOrdinal(kHostedAppId, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kHostedAppId, conflict_ordinal);

  sorting->SetPageOrdinal(kPackagedApp1Id, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kPackagedApp1Id, conflict_ordinal);

  sorting->SetPageOrdinal(kPackagedApp2Id, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kPackagedApp2Id, conflict_ordinal);

  scoped_ptr<app_list::AppListModel::Apps> model(
      new app_list::AppListModel::Apps);
  AppsModelBuilder builder(profile_.get(), model.get(), NULL);
  builder.Build();

  // By default, conflicted items are sorted by their app ids.
  EXPECT_EQ(std::string("Hosted App,Packaged App 1,Packaged App 2"),
            GetModelContent(model.get()));

  // Move hosted app between app1 and app2 and it should not crash.
  service_->OnExtensionMoved(kHostedAppId, kPackagedApp1Id, kPackagedApp2Id);
  EXPECT_EQ(std::string("Packaged App 1,Hosted App,Packaged App 2"),
            GetModelContent(model.get()));
}
