// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_provider.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/drive/drive_app_mapping.h"
#include "chrome/browser/apps/drive/drive_service_bridge.h"
#include "chrome/browser/drive/drive_app_registry.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"

using extensions::AppLaunchInfo;
using extensions::Extension;
using extensions::ExtensionRegistry;

namespace {

const char kDriveAppId[] = "drive_app_id";
const char kDriveAppName[] = "Fake Drive App";
const char kLaunchUrl[] = "http://example.com/drive";

// App id of hosted_app.crx.
const char kChromeAppId[] = "kbmnembihfiondgfjekmnmcbddelicoi";

// Stub drive service bridge.
class TestDriveServiceBridge : public DriveServiceBridge {
 public:
  explicit TestDriveServiceBridge(drive::DriveAppRegistry* registry)
      : registry_(registry) {}
  virtual ~TestDriveServiceBridge() {}

  virtual drive::DriveAppRegistry* GetAppRegistry() OVERRIDE {
    return registry_;
  }

 private:
  drive::DriveAppRegistry* registry_;

  DISALLOW_COPY_AND_ASSIGN(TestDriveServiceBridge);
};

}  // namespace

class DriveAppProviderTest : public ExtensionBrowserTest,
                             public extensions::InstallObserver {
 public:
  DriveAppProviderTest() {}
  virtual ~DriveAppProviderTest() {}

  // ExtensionBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionBrowserTest::SetUpOnMainThread();

    fake_drive_service_.reset(new drive::FakeDriveService);
    fake_drive_service_->LoadAppListForDriveApi("drive/applist_empty.json");
    apps_registry_.reset(
        new drive::DriveAppRegistry(fake_drive_service_.get()));

    provider_.reset(new DriveAppProvider(profile()));
    provider_->SetDriveServiceBridgeForTest(
        make_scoped_ptr(new TestDriveServiceBridge(apps_registry_.get()))
            .PassAs<DriveServiceBridge>());

    // The DriveAppProvider in AppListSyncalbeService interferes with the
    // test. So resets it.
    app_list::AppListSyncableServiceFactory::GetForProfile(profile())
        ->ResetDriveAppProviderForTest();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    provider_.reset();
    apps_registry_.reset();
    fake_drive_service_.reset();

    ExtensionBrowserTest::TearDownOnMainThread();
  }

  const Extension* InstallChromeApp(int expected_change) {
    base::FilePath test_data_path;
    if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_path)) {
      ADD_FAILURE();
      return NULL;
    }
    test_data_path =
        test_data_path.AppendASCII("extensions").AppendASCII("hosted_app.crx");
    const Extension* extension =
        InstallExtension(test_data_path, expected_change);
    return extension;
  }

  void RefreshDriveAppRegistry() {
    apps_registry_->Update();
    content::RunAllPendingInMessageLoop();
  }

  void WaitForPendingDriveAppConverters() {
    DCHECK(!runner_.get());

    if (provider_->pending_converters_.empty())
      return;

    runner_ = new content::MessageLoopRunner;

    pending_drive_app_converter_check_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(50),
        base::Bind(&DriveAppProviderTest::OnPendingDriveAppConverterCheckTimer,
                   base::Unretained(this)));

    runner_->Run();

    pending_drive_app_converter_check_timer_.Stop();
    runner_ = NULL;
  }

  void InstallUserUrlApp(const std::string& url) {
    DCHECK(!runner_.get());
    runner_ = new content::MessageLoopRunner;

    WebApplicationInfo web_app;
    web_app.title = base::ASCIIToUTF16("User installed Url app");
    web_app.app_url = GURL(url);

    scoped_refptr<extensions::CrxInstaller> crx_installer =
        extensions::CrxInstaller::CreateSilent(
            extensions::ExtensionSystem::Get(profile())->extension_service());
    crx_installer->set_creation_flags(Extension::FROM_BOOKMARK);
    extensions::InstallTracker::Get(profile())->AddObserver(this);
    crx_installer->InstallWebApp(web_app);

    runner_->Run();
    runner_ = NULL;
    extensions::InstallTracker::Get(profile())->RemoveObserver(this);

    content::RunAllPendingInMessageLoop();
  }

  bool HasPendingConverters() const {
    return !provider_->pending_converters_.empty();
  }

  drive::FakeDriveService* fake_drive_service() {
    return fake_drive_service_.get();
  }
  DriveAppProvider* provider() { return provider_.get(); }
  DriveAppMapping* mapping() { return provider_->mapping_.get(); }

 private:
  void OnPendingDriveAppConverterCheckTimer() {
    if (!HasPendingConverters())
      runner_->Quit();
  }

  // extensions::InstallObserver
  virtual void OnFinishCrxInstall(const std::string& extension_id,
                                  bool success) OVERRIDE {
    runner_->Quit();
  }

  scoped_ptr<drive::FakeDriveService> fake_drive_service_;
  scoped_ptr<drive::DriveAppRegistry> apps_registry_;
  scoped_ptr<DriveAppProvider> provider_;

  base::RepeatingTimer<DriveAppProviderTest>
      pending_drive_app_converter_check_timer_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(DriveAppProviderTest);
};

// A Drive app maps to an existing Chrome app that has a matching id.
// Uninstalling the chrome app would also disconnect the drive app.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, ExistingChromeApp) {
  // Prepare an existing chrome app.
  const Extension* chrome_app = InstallChromeApp(1);
  ASSERT_TRUE(chrome_app);

  // Prepare a Drive app that matches the chrome app id.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, chrome_app->id(), kLaunchUrl);
  RefreshDriveAppRegistry();
  EXPECT_FALSE(HasPendingConverters());

  // The Drive app should use the matching chrome app.
  EXPECT_EQ(chrome_app->id(), mapping()->GetChromeApp(kDriveAppId));
  EXPECT_FALSE(mapping()->IsChromeAppGenerated(chrome_app->id()));

  // Unintalling chrome app should disconnect the Drive app on server.
  EXPECT_TRUE(fake_drive_service()->HasApp(kDriveAppId));
  UninstallExtension(chrome_app->id());
  EXPECT_FALSE(fake_drive_service()->HasApp(kDriveAppId));
}

// A Drive app creates an URL app when no matching Chrome app presents.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, CreateUrlApp) {
  // Prepare a Drive app with no underlying chrome app.
  fake_drive_service()->AddApp(kDriveAppId, kDriveAppName, "", kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // An Url app should be created.
  const Extension* chrome_app =
      ExtensionRegistry::Get(profile())->GetExtensionById(
          mapping()->GetChromeApp(kDriveAppId), ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(chrome_app);
  EXPECT_EQ(kDriveAppName, chrome_app->name());
  EXPECT_TRUE(chrome_app->is_hosted_app());
  EXPECT_TRUE(chrome_app->from_bookmark());
  EXPECT_EQ(GURL(kLaunchUrl), AppLaunchInfo::GetLaunchWebURL(chrome_app));

  EXPECT_EQ(chrome_app->id(), mapping()->GetChromeApp(kDriveAppId));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(chrome_app->id()));

  // Unintalling the chrome app should disconnect the Drive app on server.
  EXPECT_TRUE(fake_drive_service()->HasApp(kDriveAppId));
  UninstallExtension(chrome_app->id());
  EXPECT_FALSE(fake_drive_service()->HasApp(kDriveAppId));
}

// A matching Chrome app replaces the created URL app.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, MatchingChromeAppInstalled) {
  // Prepare a Drive app that matches the not-yet-installed kChromeAppId.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // An Url app should be created.
  const Extension* url_app =
      ExtensionRegistry::Get(profile())->GetExtensionById(
          mapping()->GetChromeApp(kDriveAppId), ExtensionRegistry::EVERYTHING);
  EXPECT_TRUE(url_app->is_hosted_app());
  EXPECT_TRUE(url_app->from_bookmark());

  const std::string url_app_id = url_app->id();
  EXPECT_NE(kChromeAppId, url_app_id);
  EXPECT_EQ(url_app_id, mapping()->GetChromeApp(kDriveAppId));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(url_app_id));

  // Installs a chrome app with matching id.
  InstallChromeApp(0);

  // The Drive app should be mapped to chrome app.
  EXPECT_EQ(kChromeAppId, mapping()->GetChromeApp(kDriveAppId));
  EXPECT_FALSE(mapping()->IsChromeAppGenerated(kChromeAppId));

  // Url app should be auto uninstalled.
  EXPECT_FALSE(ExtensionRegistry::Get(profile())->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING));
}

// Tests that the corresponding URL app is uninstalled when a Drive app is
// disconnected.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest,
                       DisconnectDriveAppUninstallUrlApp) {
  // Prepare a Drive app that matches the not-yet-installed kChromeAppId.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // Url app is created.
  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING));

  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  RefreshDriveAppRegistry();

  // Url app is auto uninstalled.
  EXPECT_FALSE(ExtensionRegistry::Get(profile())->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING));
}

// Tests that the matching Chrome app is preserved when a Drive app is
// disconnected.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest,
                       DisconnectDriveAppPreserveChromeApp) {
  // Prepare an existing chrome app.
  const Extension* chrome_app = InstallChromeApp(1);
  ASSERT_TRUE(chrome_app);

  // Prepare a Drive app that matches the chrome app id.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  EXPECT_FALSE(HasPendingConverters());

  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  RefreshDriveAppRegistry();

  // Chrome app is still present after the Drive app is disconnected.
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->GetExtensionById(
      kChromeAppId, ExtensionRegistry::EVERYTHING));
}

// The "generated" flag of an app should stay across Drive app conversion.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, KeepGeneratedFlagBetweenUpdates) {
  // Prepare a Drive app with no underlying chrome app.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(url_app_id));

  // Change name to trigger an update.
  const char kChangedName[] = "Changed name";
  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  fake_drive_service()->AddApp(
      kDriveAppId, kChangedName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // It should still map to the same url app id and tagged as generated.
  EXPECT_EQ(url_app_id, mapping()->GetChromeApp(kDriveAppId));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(url_app_id));
}

// A new URL app replaces the existing one and keeps existing// position when a
// Drive app changes its name or URL.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, DriveAppChanged) {
  // Prepare a Drive app with no underlying chrome app.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // An Url app should be created.
  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  const Extension* url_app =
      ExtensionRegistry::Get(profile())
          ->GetExtensionById(url_app_id, ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(url_app);
  EXPECT_EQ(kDriveAppName, url_app->name());
  EXPECT_TRUE(url_app->is_hosted_app());
  EXPECT_TRUE(url_app->from_bookmark());
  EXPECT_EQ(GURL(kLaunchUrl), AppLaunchInfo::GetLaunchWebURL(url_app));
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(url_app_id));

  // Register the Drive app with a different name and URL.
  const char kAnotherName[] = "Another drive app name";
  const char kAnotherLaunchUrl[] = "http://example.com/another_end_point";
  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  fake_drive_service()->AddApp(
      kDriveAppId, kAnotherName, kChromeAppId, kAnotherLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // Old URL app should be auto uninstalled.
  url_app = ExtensionRegistry::Get(profile())
                ->GetExtensionById(url_app_id, ExtensionRegistry::EVERYTHING);
  EXPECT_FALSE(url_app);

  // New URL app should be used.
  const std::string new_url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_NE(new_url_app_id, url_app_id);
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(new_url_app_id));

  const Extension* new_url_app =
      ExtensionRegistry::Get(profile())
          ->GetExtensionById(new_url_app_id, ExtensionRegistry::EVERYTHING);
  ASSERT_TRUE(new_url_app);
  EXPECT_EQ(kAnotherName, new_url_app->name());
  EXPECT_TRUE(new_url_app->is_hosted_app());
  EXPECT_TRUE(new_url_app->from_bookmark());
  EXPECT_EQ(GURL(kAnotherLaunchUrl),
            AppLaunchInfo::GetLaunchWebURL(new_url_app));
}

// An existing URL app is not changed when underlying drive app data (name and
// URL) is not changed.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, NoChange) {
  // Prepare one Drive app.
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  const Extension* url_app =
      ExtensionRegistry::Get(profile())
          ->GetExtensionById(url_app_id, ExtensionRegistry::EVERYTHING);

  // Refresh with no actual change.
  RefreshDriveAppRegistry();
  EXPECT_FALSE(HasPendingConverters());

  // Url app should remain unchanged.
  const std::string new_url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_EQ(new_url_app_id, url_app_id);

  const Extension* new_url_app =
      ExtensionRegistry::Get(profile())
          ->GetExtensionById(new_url_app_id, ExtensionRegistry::EVERYTHING);
  EXPECT_EQ(url_app, new_url_app);
}

// User installed url app before Drive app conversion should not be tagged
// as generated and not auto uninstalled.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, UserInstalledBeforeDriveApp) {
  InstallUserUrlApp(kLaunchUrl);

  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_FALSE(mapping()->IsChromeAppGenerated(url_app_id));

  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  RefreshDriveAppRegistry();

  // Url app is still present after the Drive app is disconnected.
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING));
}

// Similar to UserInstalledBeforeDriveApp but test the case where user
// installation happens after Drive app conversion.
IN_PROC_BROWSER_TEST_F(DriveAppProviderTest, UserInstalledAfterDriveApp) {
  fake_drive_service()->AddApp(
      kDriveAppId, kDriveAppName, kChromeAppId, kLaunchUrl);
  RefreshDriveAppRegistry();
  WaitForPendingDriveAppConverters();

  // Drive app converted and tagged as generated.
  const std::string url_app_id = mapping()->GetChromeApp(kDriveAppId);
  EXPECT_TRUE(mapping()->IsChromeAppGenerated(url_app_id));

  // User installation resets the generated flag.
  InstallUserUrlApp(kLaunchUrl);
  EXPECT_FALSE(mapping()->IsChromeAppGenerated(url_app_id));

  fake_drive_service()->RemoveAppByProductId(kChromeAppId);
  RefreshDriveAppRegistry();

  // Url app is still present after the Drive app is disconnected.
  EXPECT_TRUE(ExtensionRegistry::Get(profile())->GetExtensionById(
      url_app_id, ExtensionRegistry::EVERYTHING));
}
