// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_model_builder.h"

#include <vector>

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/app_list/test/fake_app_list_model_updater.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/extension_system.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

using crostini::CrostiniTestHelper;

namespace {

constexpr char kRootFolderName[] = "Linux apps";
constexpr char kDummpyApp1Name[] = "dummy1";
constexpr char kDummpyApp2Id[] = "dummy2";
constexpr char kDummpyApp2Name[] = "dummy2";
constexpr char kAppNewName[] = "new name";
constexpr char kBananaAppId[] = "banana";
constexpr char kBananaAppName[] = "banana app name";

// Returns map of items, key-ed by id.
std::map<std::string, ChromeAppListItem*> GetAppListItems(
    AppListModelUpdater* model_updater) {
  std::map<std::string, ChromeAppListItem*> result;
  for (size_t i = 0; i < model_updater->ItemCount(); ++i) {
    ChromeAppListItem* item = model_updater->ItemAtForTest(i);
    result[item->id()] = item;
  }
  return result;
}

std::vector<std::string> GetAppIds(AppListModelUpdater* model_updater) {
  std::vector<std::string> result;
  for (auto item : GetAppListItems(model_updater))
    result.push_back(item.first);
  return result;
}

// This also includes parent folder name, if applicable.
std::vector<std::string> GetAppNames(AppListModelUpdater* model_updater) {
  std::vector<std::string> result;
  std::map<std::string, ChromeAppListItem*> items =
      GetAppListItems(model_updater);
  for (auto item : items) {
    const std::string folder_id = item.second->folder_id();
    if (folder_id.empty()) {
      result.push_back(item.second->name());
      continue;
    }
    ChromeAppListItem* parent = items[folder_id];
    DCHECK(parent && parent->is_folder());
    result.push_back(parent->name() + "/" + item.second->name());
  }
  return result;
}

std::string GetFullName(const std::string& app_name) {
  return std::string(kRootFolderName) + "/" + app_name;
}

std::vector<std::string> AppendRootFolderId(
    const std::vector<std::string> ids) {
  std::vector<std::string> result = ids;
  result.emplace_back(crostini::kCrostiniFolderId);
  return result;
}

// For testing purposes, we want to pretend there are only crostini apps on the
// system. This method removes the others.
void RemoveNonCrostiniApps(app_list::AppListSyncableService* sync_service) {
  std::vector<std::string> existing_item_ids;
  for (const auto& pair : sync_service->sync_items()) {
    existing_item_ids.emplace_back(pair.first);
  }
  for (const std::string& id : existing_item_ids) {
    if (id == crostini::kCrostiniFolderId ||
        id == crostini::kCrostiniTerminalId) {
      continue;
    }
    sync_service->RemoveItem(id);
  }
}

}  // namespace

class CrostiniAppModelBuilderTest : public AppListTestBase {
 public:
  CrostiniAppModelBuilderTest() {}
  ~CrostiniAppModelBuilderTest() override {}

  void SetUp() override {
    AppListTestBase::SetUp();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile());
    model_updater_factory_scope_ = std::make_unique<
        app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>(
        base::BindRepeating([]() -> std::unique_ptr<AppListModelUpdater> {
          return std::make_unique<FakeAppListModelUpdater>();
        }));
    CreateBuilder();
  }

  void TearDown() override {
    ResetBuilder();
    test_helper_.reset();
    AppListTestBase::TearDown();
  }

 protected:
  AppListModelUpdater* GetModelUpdater() const {
    return sync_service_->GetModelUpdater();
  }

  size_t GetModelItemCount() const {
    return sync_service_->GetModelUpdater()->ItemCount();
  }

  void CreateBuilder() {
    controller_ = std::make_unique<test::TestAppListControllerDelegate>();
    builder_ = std::make_unique<CrostiniAppModelBuilder>(controller_.get());
    sync_service_ = std::make_unique<app_list::AppListSyncableService>(
        profile_.get(), extensions::ExtensionSystem::Get(profile_.get()));
    RemoveNonCrostiniApps(sync_service_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    sync_service_.reset();
  }

  crostini::CrostiniRegistryService* RegistryService() {
    return crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  }

  std::string TerminalAppName() {
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  }

  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<app_list::AppListSyncableService> sync_service_;
  std::unique_ptr<CrostiniAppModelBuilder> builder_;
  std::unique_ptr<CrostiniTestHelper> test_helper_;

 private:
  std::unique_ptr<
      app_list::AppListSyncableService::ScopedModelUpdaterFactoryForTest>
      model_updater_factory_scope_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniAppModelBuilderTest);
};

// Test that the Terminal app is only shown when Crostini is enabled
TEST_F(CrostiniAppModelBuilderTest, EnableAndDisableCrostini) {
  // Reset things so we start with Crostini not enabled.
  ResetBuilder();
  test_helper_.reset();
  test_helper_ = std::make_unique<CrostiniTestHelper>(
      profile(), /*enable_crostini=*/false);
  CreateBuilder();

  EXPECT_EQ(0u, GetModelItemCount());

  CrostiniTestHelper::EnableCrostini(profile());
  // Root folder + terminal app.
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAre(crostini::kCrostiniFolderId,
                                            crostini::kCrostiniTerminalId));
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(kRootFolderName,
                                            GetFullName(TerminalAppName())));

  CrostiniTestHelper::DisableCrostini(profile());
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::ElementsAre(crostini::kCrostiniFolderId));
}

TEST_F(CrostiniAppModelBuilderTest, AppInstallation) {
  // Root folder + terminal app.
  EXPECT_EQ(2u, GetModelItemCount());

  test_helper_->SetupDummyApps();
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAreArray(AppendRootFolderId(
                  RegistryService()->GetRegisteredAppIds())));
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(
                  kRootFolderName, GetFullName(TerminalAppName()),
                  GetFullName(kDummpyApp1Name), GetFullName(kDummpyApp2Name)));

  test_helper_->AddApp(
      CrostiniTestHelper::BasicApp(kBananaAppId, kBananaAppName));
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAreArray(AppendRootFolderId(
                  RegistryService()->GetRegisteredAppIds())));
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(
                  kRootFolderName, GetFullName(TerminalAppName()),
                  GetFullName(kDummpyApp1Name), GetFullName(kDummpyApp2Name),
                  GetFullName(kBananaAppName)));
}

// Test that the app model builder correctly picks up changes to existing apps.
TEST_F(CrostiniAppModelBuilderTest, UpdateApps) {
  test_helper_->SetupDummyApps();
  // 3 apps + root folder.
  EXPECT_EQ(4u, GetModelItemCount());

  // Setting NoDisplay to true should hide an app.
  vm_tools::apps::App dummy1 = test_helper_->GetApp(0);
  dummy1.set_no_display(true);
  test_helper_->AddApp(dummy1);
  EXPECT_EQ(3u, GetModelItemCount());
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAre(
                  crostini::kCrostiniFolderId, crostini::kCrostiniTerminalId,
                  CrostiniTestHelper::GenerateAppId(kDummpyApp2Id)));

  // Setting NoDisplay to false should unhide an app.
  dummy1.set_no_display(false);
  test_helper_->AddApp(dummy1);
  EXPECT_EQ(4u, GetModelItemCount());
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAreArray(AppendRootFolderId(
                  RegistryService()->GetRegisteredAppIds())));

  // Changes to app names should be detected.
  vm_tools::apps::App dummy2 =
      CrostiniTestHelper::BasicApp(kDummpyApp2Id, kAppNewName);
  test_helper_->AddApp(dummy2);
  EXPECT_EQ(4u, GetModelItemCount());
  EXPECT_THAT(GetAppIds(GetModelUpdater()),
              testing::UnorderedElementsAreArray(AppendRootFolderId(
                  RegistryService()->GetRegisteredAppIds())));
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(
                  kRootFolderName, GetFullName(TerminalAppName()),
                  GetFullName(kDummpyApp1Name), GetFullName(kAppNewName)));
}

// Test that the app model builder handles removed apps
TEST_F(CrostiniAppModelBuilderTest, RemoveApps) {
  test_helper_->SetupDummyApps();
  // 3 apps + root folder.
  EXPECT_EQ(4u, GetModelItemCount());

  // Remove dummy1
  test_helper_->RemoveApp(0);
  EXPECT_EQ(3u, GetModelItemCount());

  // Remove dummy2
  test_helper_->RemoveApp(0);
  EXPECT_EQ(2u, GetModelItemCount());
}

// Tests that the crostini folder is recreated on demand.
TEST_F(CrostiniAppModelBuilderTest, RecreateFolder) {
  CrostiniTestHelper::EnableCrostini(profile());
  // Root folder + terminal app.
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(kRootFolderName,
                                            GetFullName(TerminalAppName())));

  // Move the terminal out and delete the old folder.
  GetModelUpdater()->MoveItemToFolder(crostini::kCrostiniTerminalId, "");
  GetModelUpdater()->RemoveItem(crostini::kCrostiniFolderId);
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(TerminalAppName()));

  // Adding a new app should recreate the folder.
  test_helper_->AddApp(
      CrostiniTestHelper::BasicApp(kDummpyApp2Id, kDummpyApp2Name));
  EXPECT_THAT(GetAppNames(GetModelUpdater()),
              testing::UnorderedElementsAre(TerminalAppName(), kRootFolderName,
                                            GetFullName(kDummpyApp2Name)));
}

// Test that the Terminal app is removed when Crostini is disabled.
TEST_F(CrostiniAppModelBuilderTest, DisableCrostini) {
  test_helper_->SetupDummyApps();
  // 3 apps + root folder.
  EXPECT_EQ(4u, GetModelItemCount());

  // The uninstall flow removes all apps before setting the CrostiniEnabled pref
  // to false, so we need to do that explicitly too.
  RegistryService()->ClearApplicationList(crostini::kCrostiniDefaultVmName, "");
  CrostiniTestHelper::DisableCrostini(profile());
  // Root folder is left. We rely on default handling of empty folder.
  EXPECT_EQ(1u, GetModelItemCount());
}
