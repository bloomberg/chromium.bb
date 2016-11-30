// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/arc/arc_default_app_list.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace {

constexpr char kTestPackageName[] = "fake.package.name2";

class FakeAppIconLoaderDelegate : public AppIconLoaderDelegate {
 public:
  FakeAppIconLoaderDelegate() = default;
  ~FakeAppIconLoaderDelegate() override = default;

  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override {
    app_id_ = app_id;
    image_ = image;
    ++update_image_cnt_;
  }

  size_t update_image_cnt() const { return update_image_cnt_; }

  const std::string& app_id() const { return app_id_; }

  const gfx::ImageSkia& image() { return image_; }

 private:
  size_t update_image_cnt_ = 0;
  std::string app_id_;
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppIconLoaderDelegate);
};

void WaitForIconReady(ArcAppListPrefs* prefs,
                      const std::string& app_id,
                      ui::ScaleFactor scale_factor) {
  const base::FilePath icon_path = prefs->GetIconPath(app_id, scale_factor);
  // Process pending tasks. This performs multiple thread hops, so we need
  // to run it continuously until it is resolved.
  do {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  } while (!base::PathExists(icon_path));
}

}  // namespace

class ArcAppModelBuilderTest : public AppListTestBase {
 public:
  ArcAppModelBuilderTest() = default;
  ~ArcAppModelBuilderTest() override {
    // Release profile file in order to keep right sequence.
    profile_.reset();
  }

  void SetUp() override {
    AppListTestBase::SetUp();
    OnBeforeArcTestSetup();
    arc_test_.SetUp(profile_.get());
    CreateBuilder();

    // Validating decoded content does not fit well for unit tests.
    ArcAppIcon::DisableSafeDecodingForTesting();
  }

  void TearDown() override {
    arc_test_.TearDown();
    ResetBuilder();
  }

 protected:
  // Notifies that initial preparation is done, profile is ready and it is time
  // to initialize Arc subsystem.
  virtual void OnBeforeArcTestSetup() {}

  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    ResetBuilder();  // Destroy any existing builder in the correct order.

    model_.reset(new app_list::AppListModel);
    controller_.reset(new test::TestAppListControllerDelegate);
    builder_.reset(new ArcAppModelBuilder(controller_.get()));
    builder_->InitializeWithProfile(profile_.get(), model_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_.reset();
  }

  size_t GetArcItemCount() const {
    size_t arc_count = 0;
    const size_t count = model_->top_level_item_list()->item_count();
    for (size_t i = 0; i < count; ++i) {
      app_list::AppListItem* item = model_->top_level_item_list()->item_at(i);
      if (item->GetItemType() == ArcAppItem::kItemType)
        ++arc_count;
    }
    return arc_count;
  }

  ArcAppItem* GetArcItem(size_t index) const {
    size_t arc_count = 0;
    const size_t count = model_->top_level_item_list()->item_count();
    ArcAppItem* arc_item = nullptr;
    for (size_t i = 0; i < count; ++i) {
      app_list::AppListItem* item = model_->top_level_item_list()->item_at(i);
      if (item->GetItemType() == ArcAppItem::kItemType) {
        if (arc_count++ == index) {
          arc_item = reinterpret_cast<ArcAppItem*>(item);
          break;
        }
      }
    }
    EXPECT_NE(nullptr, arc_item);
    return arc_item;
  }

  ArcAppItem* FindArcItem(const std::string& id) const {
    const size_t count = GetArcItemCount();
    ArcAppItem* found_item = nullptr;
    for (size_t i = 0; i < count; ++i) {
      ArcAppItem* item = GetArcItem(i);
      if (item && item->id() == id) {
        DCHECK(!found_item);
        found_item = item;
      }
    }
    return found_item;
  }

  // Validate that prefs and model have right content.
  void ValidateHaveApps(const std::vector<arc::mojom::AppInfo> apps) {
    ValidateHaveAppsAndShortcuts(apps, std::vector<arc::mojom::ShortcutInfo>());
  }

  void ValidateHaveShortcuts(
      const std::vector<arc::mojom::ShortcutInfo> shortcuts) {
    ValidateHaveAppsAndShortcuts(std::vector<arc::mojom::AppInfo>(), shortcuts);
  }

  void ValidateHaveAppsAndShortcuts(
      const std::vector<arc::mojom::AppInfo> apps,
      const std::vector<arc::mojom::ShortcutInfo> shortcuts) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);
    const std::vector<std::string> ids = prefs->GetAppIds();
    ASSERT_EQ(apps.size() + shortcuts.size(), ids.size());
    ASSERT_EQ(apps.size() + shortcuts.size(), GetArcItemCount());
    // In principle, order of items is not defined.
    for (const auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      EXPECT_NE(std::find(ids.begin(), ids.end(), id), ids.end());
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(app.name, app_info->name);
      EXPECT_EQ(app.package_name, app_info->package_name);
      EXPECT_EQ(app.activity, app_info->activity);

      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(app.name, app_item->GetDisplayName());
    }

    for (auto& shortcut : shortcuts) {
      const std::string id = ArcAppTest::GetAppId(shortcut);
      EXPECT_NE(std::find(ids.begin(), ids.end(), id), ids.end());
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(shortcut.name, app_info->name);
      EXPECT_EQ(shortcut.package_name, app_info->package_name);
      EXPECT_EQ(shortcut.intent_uri, app_info->intent_uri);

      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(shortcut.name, app_item->GetDisplayName());
    }
  }

  // Validate that prefs have right packages.
  void ValidateHavePackages(
      const std::vector<arc::mojom::ArcPackageInfo> packages) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);
    const std::vector<std::string> pref_packages =
        prefs->GetPackagesFromPrefs();
    ASSERT_EQ(packages.size(), pref_packages.size());
    for (const auto& package : packages) {
      const std::string package_name = package.package_name;
      std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info =
          prefs->GetPackage(package_name);
      ASSERT_NE(nullptr, package_info.get());
      EXPECT_EQ(package.last_backup_android_id,
                package_info->last_backup_android_id);
      EXPECT_EQ(package.last_backup_time, package_info->last_backup_time);
      EXPECT_EQ(package.sync, package_info->should_sync);
    }
  }

  // Validate that requested apps have required ready state and other apps have
  // opposite state.
  void ValidateAppReadyState(const std::vector<arc::mojom::AppInfo> apps,
                             bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      std::vector<std::string>::iterator it_id = std::find(ids.begin(),
                                                           ids.end(),
                                                           id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
    }

    // Process the rest of the apps.
    for (auto& id : ids) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_NE(ready, app_info->ready);
    }
  }

  // Validate that requested shortcuts have required ready state
  void ValidateShortcutReadyState(
      const std::vector<arc::mojom::ShortcutInfo> shortcuts,
      bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& shortcut : shortcuts) {
      const std::string id = ArcAppTest::GetAppId(shortcut);
      std::vector<std::string>::iterator it_id =
          std::find(ids.begin(), ids.end(), id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
    }
  }

  // Validates that provided image is acceptable as Arc app icon.
  void ValidateIcon(const gfx::ImageSkia& image) {
    EXPECT_EQ(app_list::kGridIconDimension, image.width());
    EXPECT_EQ(app_list::kGridIconDimension, image.height());

    const std::vector<ui::ScaleFactor>& scale_factors =
        ui::GetSupportedScaleFactors();
    for (auto& scale_factor : scale_factors) {
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      EXPECT_TRUE(image.HasRepresentation(scale));
      const gfx::ImageSkiaRep& representation = image.GetRepresentation(scale);
      EXPECT_FALSE(representation.is_null());
      EXPECT_EQ(gfx::ToCeiledInt(app_list::kGridIconDimension * scale),
          representation.pixel_width());
      EXPECT_EQ(gfx::ToCeiledInt(app_list::kGridIconDimension * scale),
          representation.pixel_height());
    }
  }

  void AddPackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.AddPackage(package);
  }

  void RemovePackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.RemovePackage(package);
  }

  AppListControllerDelegate* controller() { return controller_.get(); }

  Profile* profile() { return profile_.get(); }

  ArcAppTest* arc_test() { return &arc_test_; }

  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return arc_test_.fake_apps();
  }

  const std::vector<arc::mojom::AppInfo>& fake_default_apps() const {
    return arc_test_.fake_default_apps();
  }

  const std::vector<arc::mojom::ArcPackageInfo>& fake_packages() const {
    return arc_test_.fake_packages();
  }

  const std::vector<arc::mojom::ShortcutInfo>& fake_shortcuts() const {
    return arc_test_.fake_shortcuts();
  }

  arc::FakeAppInstance* app_instance() {
    return arc_test_.app_instance();
  }

 private:
  ArcAppTest arc_test_;
  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<ArcAppModelBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppModelBuilderTest);
};

class ArcDefaulAppTest : public ArcAppModelBuilderTest {
 public:
  ArcDefaulAppTest() = default;
  ~ArcDefaulAppTest() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    ArcDefaultAppList::UseTestAppsDirectory();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDefaulAppTest);
};

class ArcDefaulAppForManagedUserTest : public ArcDefaulAppTest {
 public:
  ArcDefaulAppForManagedUserTest() = default;
  ~ArcDefaulAppForManagedUserTest() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    ArcDefaulAppTest::OnBeforeArcTestSetup();

    policy::ProfilePolicyConnector* const connector =
        policy::ProfilePolicyConnectorFactory::GetForBrowserContext(
            profile());
    connector->OverrideIsManagedForTesting(true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDefaulAppForManagedUserTest);
};

class ArcPlayStoreAppTest : public ArcDefaulAppTest {
 public:
  ArcPlayStoreAppTest() = default;
  ~ArcPlayStoreAppTest() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    ArcDefaulAppTest::OnBeforeArcTestSetup();

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "Play Store");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "Play Store for testing");

    std::string error;
    arc_support_host_ = extensions::Extension::Create(
        base::FilePath(),
        extensions::Manifest::UNPACKED,
        manifest, extensions::Extension::NO_FLAGS,
        ArcSupportHost::kHostAppId, &error);

    ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_.get())->extension_service();
    extension_service->AddExtension(arc_support_host_.get());
  }

 private:
  scoped_refptr<extensions::Extension> arc_support_host_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreAppTest);
};

class ArcAppModelBuilderRecreate : public ArcAppModelBuilderTest {
 public:
  ArcAppModelBuilderRecreate() = default;
  ~ArcAppModelBuilderRecreate() override = default;

 protected:
  // ArcAppModelBuilderTest:
  void OnBeforeArcTestSetup() override {
    arc::ArcPackageSyncableServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppModelBuilderRecreate);
};

TEST_F(ArcAppModelBuilderTest, ArcPackagePref) {
  ValidateHavePackages(std::vector<arc::mojom::ArcPackageInfo>());
  app_instance()->SendRefreshPackageList(fake_packages());
  ValidateHavePackages(fake_packages());

  arc::mojom::ArcPackageInfo package;
  package.package_name = kTestPackageName;
  package.package_version = 2;
  package.last_backup_android_id = 2;
  package.last_backup_time = 2;
  package.sync = true;

  RemovePackage(package);
  app_instance()->SendPackageUninstalled(package.package_name);
  ValidateHavePackages(fake_packages());

  AddPackage(package);
  app_instance()->SendPackageAdded(package);
  ValidateHavePackages(fake_packages());
}

TEST_F(ArcAppModelBuilderTest, RefreshAllOnReady) {
  // There should already have been one call, when the interface was
  // registered.
  EXPECT_EQ(1, app_instance()->refresh_app_list_count());
  app_instance()->RefreshAppList();
  EXPECT_EQ(2, app_instance()->refresh_app_list_count());
}

TEST_F(ArcAppModelBuilderTest, RefreshAllFillsContent) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
}

TEST_F(ArcAppModelBuilderTest, InstallShortcut) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());

  app_instance()->SendInstallShortcuts(fake_shortcuts());
  ValidateHaveShortcuts(fake_shortcuts());
}

TEST_F(ArcAppModelBuilderTest, RefreshAllPreservesShortcut) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());

  app_instance()->SendInstallShortcuts(fake_shortcuts());
  ValidateHaveAppsAndShortcuts(fake_apps(), fake_shortcuts());

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveAppsAndShortcuts(fake_apps(), fake_shortcuts());
}

TEST_F(ArcAppModelBuilderTest, MultipleRefreshAll) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  // Send info about all fake apps except last.
  std::vector<arc::mojom::AppInfo> apps1(fake_apps().begin(),
                                         fake_apps().end() - 1);
  app_instance()->SendRefreshAppList(apps1);
  // At this point all apps (except last) should exist and be ready.
  ValidateHaveApps(apps1);
  ValidateAppReadyState(apps1, true);

  // Send info about all fake apps except first.
  std::vector<arc::mojom::AppInfo> apps2(fake_apps().begin() + 1,
                                         fake_apps().end());
  app_instance()->SendRefreshAppList(apps2);
  // At this point all apps should exist but first one should be non-ready.
  ValidateHaveApps(apps2);
  ValidateAppReadyState(apps2, true);

  // Send info about all fake apps.
  app_instance()->SendRefreshAppList(fake_apps());
  // At this point all apps should exist and be ready.
  ValidateHaveApps(fake_apps());
  ValidateAppReadyState(fake_apps(), true);

  // Send info no app available.
  std::vector<arc::mojom::AppInfo> no_apps;
  app_instance()->SendRefreshAppList(no_apps);
  // At this point no app should exist.
  ValidateHaveApps(no_apps);
}

TEST_F(ArcAppModelBuilderTest, StopStartServicePreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  EXPECT_EQ(0u, GetArcItemCount());
  EXPECT_EQ(0u, prefs->GetAppIds().size());

  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // Stopping service does not delete items. It makes them non-ready.
  arc_test()->StopArcInstance();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Refreshing app list makes apps available.
  app_instance()->SendRefreshAppList(fake_apps());
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), true);
}

TEST_F(ArcAppModelBuilderTest, StopStartServicePreserveShortcuts) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  EXPECT_EQ(0u, GetArcItemCount());
  EXPECT_EQ(0u, prefs->GetAppIds().size());

  app_instance()->SendInstallShortcuts(fake_shortcuts());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_shortcuts().size(), ids.size());
  ValidateShortcutReadyState(fake_shortcuts(), true);

  // Stopping service does not delete items. It makes them non-ready.
  arc_test()->StopArcInstance();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), false);

  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), false);

  // Refreshing app list makes apps available.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateShortcutReadyState(fake_shortcuts(), true);
}

TEST_F(ArcAppModelBuilderTest, RestartPreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Start from scratch and fill with apps.
  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // This recreates model and ARC apps will be read from prefs.
  arc_test()->StopArcInstance();
  CreateBuilder();
  ValidateAppReadyState(fake_apps(), false);
}

TEST_F(ArcAppModelBuilderTest, RestartPreserveShortcuts) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Start from scratch and install shortcuts.
  app_instance()->SendInstallShortcuts(fake_shortcuts());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateShortcutReadyState(fake_shortcuts(), true);

  // This recreates model and ARC apps and shortcuts will be read from prefs.
  arc_test()->StopArcInstance();
  CreateBuilder();
  ValidateShortcutReadyState(fake_shortcuts(), false);
}

TEST_F(ArcAppModelBuilderTest, LaunchApps) {
  // Disable attempts to dismiss app launcher view.
  ChromeAppListItem::OverrideAppListControllerDelegateForTesting(controller());

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  // Simulate item activate.
  const arc::mojom::AppInfo& app_first = fake_apps()[0];
  const arc::mojom::AppInfo& app_last = fake_apps()[0];
  ArcAppItem* item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ArcAppItem* item_last = FindArcItem(ArcAppTest::GetAppId(app_last));
  ASSERT_NE(nullptr, item_first);
  ASSERT_NE(nullptr, item_last);
  item_first->Activate(0);
  item_last->Activate(0);
  item_first->Activate(0);

  const std::vector<std::unique_ptr<arc::FakeAppInstance::Request>>&
      launch_requests = app_instance()->launch_requests();
  ASSERT_EQ(3u, launch_requests.size());
  EXPECT_TRUE(launch_requests[0]->IsForApp(app_first));
  EXPECT_TRUE(launch_requests[1]->IsForApp(app_last));
  EXPECT_TRUE(launch_requests[2]->IsForApp(app_first));

  // Test an attempt to launch of a not-ready app.
  arc_test()->StopArcInstance();
  item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ASSERT_NE(nullptr, item_first);
  size_t launch_request_count_before = app_instance()->launch_requests().size();
  item_first->Activate(0);
  // Number of launch requests must not change.
  EXPECT_EQ(launch_request_count_before,
            app_instance()->launch_requests().size());
}

TEST_F(ArcAppModelBuilderTest, LaunchShortcuts) {
  // Disable attempts to dismiss app launcher view.
  ChromeAppListItem::OverrideAppListControllerDelegateForTesting(controller());

  app_instance()->RefreshAppList();
  app_instance()->SendInstallShortcuts(fake_shortcuts());

  // Simulate item activate.
  const arc::mojom::ShortcutInfo& app_first = fake_shortcuts()[0];
  const arc::mojom::ShortcutInfo& app_last = fake_shortcuts()[0];
  ArcAppItem* item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ArcAppItem* item_last = FindArcItem(ArcAppTest::GetAppId(app_last));
  ASSERT_NE(nullptr, item_first);
  ASSERT_NE(nullptr, item_last);
  item_first->Activate(0);
  item_last->Activate(0);
  item_first->Activate(0);

  const std::vector<std::string>& launch_intents =
      app_instance()->launch_intents();
  ASSERT_EQ(3u, launch_intents.size());
  EXPECT_EQ(app_first.intent_uri, launch_intents[0]);
  EXPECT_EQ(app_last.intent_uri, launch_intents[1]);
  EXPECT_EQ(app_first.intent_uri, launch_intents[2]);

  // Test an attempt to launch of a not-ready shortcut.
  arc_test()->StopArcInstance();
  item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ASSERT_NE(nullptr, item_first);
  size_t launch_request_count_before = app_instance()->launch_intents().size();
  item_first->Activate(0);
  // Number of launch requests must not change.
  EXPECT_EQ(launch_request_count_before,
            app_instance()->launch_intents().size());
}

TEST_F(ArcAppModelBuilderTest, RequestIcons) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  // Validate that no icon exists at the beginning and request icon for
  // each supported scale factor. This will start asynchronous loading.
  uint32_t expected_mask = 0;
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (auto& scale_factor : scale_factors) {
    expected_mask |= 1 <<  scale_factor;
    for (auto& app : fake_apps()) {
      ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(app));
      ASSERT_NE(nullptr, app_item);
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      app_item->icon().GetRepresentation(scale);

      // This does not result in an icon being loaded, so WaitForIconReady
      // cannot be used.
      content::BrowserThread::GetBlockingPool()->FlushForTesting();
      base::RunLoop().RunUntilIdle();
    }
  }

  const size_t expected_size = scale_factors.size() * fake_apps().size();

  // At this moment we should receive all requests for icon loading.
  const std::vector<std::unique_ptr<arc::FakeAppInstance::IconRequest>>&
      icon_requests = app_instance()->icon_requests();
  EXPECT_EQ(expected_size, icon_requests.size());
  std::map<std::string, uint32_t> app_masks;
  for (size_t i = 0; i < icon_requests.size(); ++i) {
    const arc::FakeAppInstance::IconRequest* icon_request =
        icon_requests[i].get();
    const std::string id = ArcAppListPrefs::GetAppId(
        icon_request->package_name(), icon_request->activity());
    // Make sure no double requests.
    EXPECT_NE(app_masks[id],
              app_masks[id] | (1 << icon_request->scale_factor()));
    app_masks[id] |= (1 << icon_request->scale_factor());
  }

  // Validate that we have a request for each icon for each supported scale
  // factor.
  EXPECT_EQ(fake_apps().size(), app_masks.size());
  for (auto& app : fake_apps()) {
    const std::string id = ArcAppTest::GetAppId(app);
    ASSERT_NE(app_masks.find(id), app_masks.end());
    EXPECT_EQ(app_masks[id], expected_mask);
  }
}

TEST_F(ArcAppModelBuilderTest, RequestShortcutIcons) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  const arc::mojom::ShortcutInfo& shortcut = fake_shortcuts()[0];
  app_instance()->SendInstallShortcut(shortcut);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Validate that no icon exists at the beginning and request icon for
  // each supported scale factor. This will start asynchronous loading.
  uint32_t expected_mask = 0;
  ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(shortcut));
  ASSERT_NE(nullptr, app_item);
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (auto& scale_factor : scale_factors) {
    expected_mask |= 1 << scale_factor;
    const float scale = ui::GetScaleForScaleFactor(scale_factor);
    const base::FilePath icon_path =
        prefs->GetIconPath(ArcAppTest::GetAppId(shortcut), scale_factor);
    EXPECT_FALSE(base::PathExists(icon_path));

    app_item->icon().GetRepresentation(scale);
    WaitForIconReady(prefs, ArcAppTest::GetAppId(shortcut), scale_factor);
  }

  // At this moment we should receive all requests for icon loading.
  const size_t expected_size = scale_factors.size();
  const std::vector<std::unique_ptr<arc::FakeAppInstance::ShortcutIconRequest>>&
      icon_requests = app_instance()->shortcut_icon_requests();
  EXPECT_EQ(expected_size, icon_requests.size());
  uint32_t app_mask = 0;
  for (size_t i = 0; i < icon_requests.size(); ++i) {
    const arc::FakeAppInstance::ShortcutIconRequest* icon_request =
        icon_requests[i].get();
    EXPECT_EQ(shortcut.icon_resource_id, icon_request->icon_resource_id());

    // Make sure no double requests.
    EXPECT_NE(app_mask, app_mask | (1 << icon_request->scale_factor()));
    app_mask |= (1 << icon_request->scale_factor());
  }

  // Validate that we have a request for each icon for each supported scale
  // factor.
  EXPECT_EQ(app_mask, expected_mask);

  // Validate all icon files are installed.
  for (auto& scale_factor : scale_factors) {
    const base::FilePath icon_path =
        prefs->GetIconPath(ArcAppTest::GetAppId(shortcut), scale_factor);
    EXPECT_TRUE(base::PathExists(icon_path));
  }
}

TEST_F(ArcAppModelBuilderTest, InstallIcon) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const arc::mojom::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];
  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  const base::FilePath icon_path = prefs->GetIconPath(ArcAppTest::GetAppId(app),
                                                      scale_factor);
  EXPECT_FALSE(base::PathExists(icon_path));

  const ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(app));
  EXPECT_NE(nullptr, app_item);
  // This initiates async loading.
  app_item->icon().GetRepresentation(scale);

  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_TRUE(app_instance()->GenerateAndSendIcon(
      app, static_cast<arc::mojom::ScaleFactor>(scale_factor), &png_data));
  WaitForIconReady(prefs, ArcAppTest::GetAppId(app), scale_factor);

  // Validate that icons are installed, have right content and icon is
  // refreshed for ARC app item.
  EXPECT_TRUE(base::PathExists(icon_path));

  std::string icon_data;
  // Read the file from disk and compare with reference data.
  EXPECT_TRUE(base::ReadFileToString(icon_path, &icon_data));
  ASSERT_EQ(icon_data, png_data);
}

TEST_F(ArcAppModelBuilderTest, RemoveAppCleanUpFolder) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const arc::mojom::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const std::string app_id = ArcAppTest::GetAppId(app);
  const base::FilePath app_path = prefs->GetAppPath(app_id);
  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];

  // No app folder by default.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(app_path));

  // Request icon, this will create app folder.
  prefs->RequestIcon(app_id, scale_factor);
  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_TRUE(app_instance()->GenerateAndSendIcon(
      app, static_cast<arc::mojom::ScaleFactor>(scale_factor), &png_data));
  WaitForIconReady(prefs, app_id, scale_factor);
  EXPECT_TRUE(base::PathExists(app_path));

  // Send empty app list. This will delete app and its folder.
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  // This cannot be WaitForIconReady since it needs to wait until the icon is
  // removed, not added.
  // Process pending tasks. This performs multiple thread hops, so we need
  // to run it continuously until it is resolved.
  do {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    base::RunLoop().RunUntilIdle();
  } while (base::PathExists(app_path));
  EXPECT_FALSE(base::PathExists(app_path));
}

TEST_F(ArcAppModelBuilderTest, LastLaunchTime) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 2));
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());

  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  // Test direct setting last launch time.
  base::Time now_time = base::Time::Now();
  prefs->SetLastLaunchTime(id1, now_time);

  app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(now_time, app_info->last_launch_time);

  // Test setting last launch time via LaunchApp.
  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  base::Time time_before = base::Time::Now();
  arc::LaunchApp(profile(), id2);
  base::Time time_after = base::Time::Now();

  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  ASSERT_LE(time_before, app_info->last_launch_time);
  ASSERT_GE(time_after, app_info->last_launch_time);
}

// Validate that arc model contains expected elements on restart.
TEST_F(ArcAppModelBuilderRecreate, AppModelRestart) {
  // No apps on initial start.
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());

  // Send info about all fake apps except last.
  std::vector<arc::mojom::AppInfo> apps1(fake_apps().begin(),
                                         fake_apps().end() - 1);
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps1);
  // Model has refreshed apps.
  ValidateHaveApps(apps1);
  EXPECT_EQ(apps1.size(), GetArcItemCount());

  // Simulate restart.
  arc_test()->TearDown();
  ResetBuilder();

  ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
      profile_.get());
  arc_test()->SetUp(profile_.get());
  CreateBuilder();

  // On restart new model contains last apps.
  ValidateHaveApps(apps1);
  EXPECT_EQ(apps1.size(), GetArcItemCount());

  // Now refresh old apps with new one.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
  EXPECT_EQ(fake_apps().size(), GetArcItemCount());
}

TEST_F(ArcPlayStoreAppTest, PlayStore) {
  // Make sure PlayStore is available.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(
      arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);

  arc::mojom::AppInfo app;
  std::vector<arc::mojom::AppInfo> apps;
  app.name = "Play Store";
  app.package_name = arc::kPlayStorePackage;
  app.activity = arc::kPlayStoreActivity;
  app.sticky = false;
  apps.push_back(app);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps);

  app_info = prefs->GetApp(arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_TRUE(app_info->ready);

  arc_test()->arc_session_manager()->DisableArc();

  app_info = prefs->GetApp(arc::kPlayStoreAppId);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);

  arc::LaunchApp(profile(), arc::kPlayStoreAppId);
  EXPECT_TRUE(arc_test()->arc_session_manager()->IsArcEnabled());
}

// TODO(crbug.com/628425) -- reenable once this test is less flaky.
TEST_F(ArcAppModelBuilderTest, DISABLED_IconLoader) {
  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(profile(),
                               app_list::kListIconSize,
                               &delegate);
  EXPECT_EQ(0UL, delegate.update_image_cnt());
  icon_loader.FetchImage(app_id);
  EXPECT_EQ(1UL, delegate.update_image_cnt());
  EXPECT_EQ(app_id, delegate.app_id());

  // Validate default image.
  ValidateIcon(delegate.image());

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  ArcAppItem* app_item = FindArcItem(app_id);
  for (auto& scale_factor : scale_factors) {
    std::string png_data;
    EXPECT_TRUE(app_instance()->GenerateAndSendIcon(
        app, static_cast<arc::mojom::ScaleFactor>(scale_factor), &png_data));
    const float scale = ui::GetScaleForScaleFactor(scale_factor);
    // Force the icon to be loaded.
    app_item->icon().GetRepresentation(scale);
    WaitForIconReady(prefs, app_id, scale_factor);
  }

  // Validate loaded image.
  EXPECT_EQ(1 + scale_factors.size(), delegate.update_image_cnt());
  EXPECT_EQ(app_id, delegate.app_id());
  ValidateIcon(delegate.image());
}

TEST_F(ArcAppModelBuilderTest, AppLauncher) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());
  ASSERT_NE(nullptr, prefs);

  // App1 is called in deferred mode, after refreshing apps.
  // App2 is never called since app is not avaialble.
  // App3 is never called immediately because app is available already.
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const arc::mojom::AppInfo& app3 = fake_apps()[2];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);
  const std::string id3 = ArcAppTest::GetAppId(app3);

  ArcAppLauncher launcher1(profile(), id1, true);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));

  ArcAppLauncher launcher3(profile(), id3, true);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  EXPECT_EQ(0u, app_instance()->launch_requests().size());

  std::vector<arc::mojom::AppInfo> apps(fake_apps().begin(),
                                        fake_apps().begin() + 2);
  app_instance()->SendRefreshAppList(apps);

  EXPECT_TRUE(launcher1.app_launched());
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(app1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_FALSE(prefs->HasObserver(&launcher1));
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  ArcAppLauncher launcher2(profile(), id2, true);
  EXPECT_TRUE(launcher2.app_launched());
  EXPECT_FALSE(prefs->HasObserver(&launcher2));
  ASSERT_EQ(2u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[1]->IsForApp(app2));
}

// Validates an app that have no launchable flag.
TEST_F(ArcAppModelBuilderTest, NonLaunchableApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  app_instance()->RefreshAppList();
  // Send all except first.
  std::vector<arc::mojom::AppInfo> apps(fake_apps().begin() + 1,
                                        fake_apps().end());
  app_instance()->SendRefreshAppList(apps);
  ValidateHaveApps(apps);

  const std::string app_id = ArcAppTest::GetAppId(fake_apps()[0]);

  EXPECT_FALSE(prefs->IsRegistered(app_id));
  EXPECT_FALSE(FindArcItem(app_id));
  app_instance()->SendTaskCreated(0, fake_apps()[0]);
  // App should not appear now in the model but should be registered.
  EXPECT_FALSE(FindArcItem(app_id));
  EXPECT_TRUE(prefs->IsRegistered(app_id));
}

TEST_F(ArcAppModelBuilderTest, ArcAppsOnPackageUpdated) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::vector<arc::mojom::AppInfo> apps = fake_apps();
  ASSERT_GE(3u, apps.size());
  apps[0].package_name = apps[2].package_name;
  apps[1].package_name = apps[2].package_name;
  // Second app should be preserved after update.
  std::vector<arc::mojom::AppInfo> apps1(apps.begin(), apps.begin() + 2);
  std::vector<arc::mojom::AppInfo> apps2(apps.begin() + 1, apps.begin() + 3);

  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(apps1);
  ValidateHaveApps(apps1);

  const std::string app_id = ArcAppTest::GetAppId(apps[1]);
  const base::Time now_time = base::Time::Now();
  prefs->SetLastLaunchTime(app_id, now_time);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_before =
      prefs->GetApp(app_id);
  ASSERT_TRUE(app_info_before);
  EXPECT_EQ(now_time, app_info_before->last_launch_time);

  app_instance()->SendPackageAppListRefreshed(apps[0].package_name, apps2);
  ValidateHaveApps(apps2);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info_after =
      prefs->GetApp(app_id);
  ASSERT_TRUE(app_info_after);
  EXPECT_EQ(now_time, app_info_after->last_launch_time);
}

TEST_F(ArcDefaulAppTest, DefaultApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  ValidateHaveApps(fake_default_apps());

  // Start normal apps. We should have apps from 2 subsets.
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  std::vector<arc::mojom::AppInfo> all_apps = fake_default_apps();
  all_apps.insert(all_apps.end(), fake_apps().begin(), fake_apps().end());
  ValidateHaveApps(all_apps);

  // However default apps are still not ready.
  for (const auto& default_app : fake_default_apps()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(
        ArcAppTest::GetAppId(default_app));
    ASSERT_TRUE(app_info);
    EXPECT_FALSE(app_info->ready);
  }

  // Install default apps.
  for (const auto& default_app : fake_default_apps()) {
    std::vector<arc::mojom::AppInfo> package_apps;
    package_apps.push_back(default_app);
    app_instance()->SendPackageAppListRefreshed(default_app.package_name,
                                                package_apps);
  }

  // And now default apps are ready.
  for (const auto& default_app : fake_default_apps()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(
        ArcAppTest::GetAppId(default_app));
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
  }

  // Uninstall first default package. Default app should go away.
  app_instance()->SendPackageUninstalled(all_apps[0].package_name);
  all_apps.erase(all_apps.begin());
  ValidateHaveApps(all_apps);

  // OptOut and default apps should exist minus first.
  arc_test()->arc_session_manager()->DisableArc();
  all_apps = fake_default_apps();
  all_apps.erase(all_apps.begin());
  ValidateHaveApps(all_apps);
}

TEST_F(ArcDefaulAppForManagedUserTest, DefaultAppsForManagedUser) {
  const ArcAppListPrefs* const prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_TRUE(prefs);

  // There is no default app for managed users except Play Store
  for (const auto& app : fake_default_apps()) {
    const std::string app_id = ArcAppTest::GetAppId(app);
    EXPECT_FALSE(prefs->IsRegistered(app_id));
    EXPECT_FALSE(prefs->GetApp(app_id));
  }
}
