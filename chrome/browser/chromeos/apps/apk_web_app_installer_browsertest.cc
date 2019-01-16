// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/apps/apk_web_app_installer.h"
#include "chrome/browser/chromeos/apps/apk_web_app_service.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"

namespace {

const char kPackageName[] = "com.google.maps";
const char kAppTitle[] = "Google Maps";
const char kAppUrl[] = "https://www.google.com/maps/";
const char kAppScope[] = "https://www.google.com/";

const std::vector<uint8_t> GetFakeIconBytes() {
  auto fake_app_instance =
      std::make_unique<arc::FakeAppInstance>(/*app_host=*/nullptr);
  std::string png_data_as_string;
  EXPECT_TRUE(fake_app_instance->GenerateIconResponse(128, /*app_icon=*/true,
                                                      &png_data_as_string));
  return std::vector<uint8_t>(png_data_as_string.begin(),
                              png_data_as_string.end());
}

}  // namespace

namespace chromeos {

class ApkWebAppInstallerBrowserTest
    : public InProcessBrowserTest,
      public extensions::ExtensionRegistryObserver,
      public ArcAppListPrefs::Observer {
 public:
  ApkWebAppInstallerBrowserTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void EnableArc() {
    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);

    arc_app_list_prefs_ = ArcAppListPrefs::Get(browser()->profile());
    DCHECK(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_ = std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);
    arc_app_list_prefs_->app_connection_holder()->SetInstance(
        app_instance_.get());
    WaitForInstanceReady(arc_app_list_prefs_->app_connection_holder());
  }

  void DisableArc() {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        app_instance_.get());
    app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  void SetUpOnMainThread() override { EnableArc(); }

  void TearDownOnMainThread() override { DisableArc(); }

  arc::mojom::ArcPackageInfoPtr GetPackage(const std::string& package_name,
                                           const std::string& app_title) {
    auto package = arc::mojom::ArcPackageInfo::New();
    package->package_name = package_name;
    package->package_version = 1;
    package->last_backup_android_id = 1;
    package->last_backup_time = 1;
    package->sync = true;
    package->system = false;

    package->web_app_info = GetWebAppInfo(app_title);

    return package;
  }

  arc::mojom::WebAppInfoPtr GetWebAppInfo(const std::string& app_title) {
    return arc::mojom::WebAppInfo::New(app_title, kAppUrl, kAppScope, 100000);
  }

  void WaitForQuit() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override {
    installed_extension_ = extension;
    is_update_ = is_update;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    reason_ = reason;
    uninstalled_extension_ = extension;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  // ArcAppListPrefs::Observer:
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override {
    EXPECT_TRUE(uninstalled);
    removed_package_ = package_name;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 protected:
  ArcAppListPrefs* arc_app_list_prefs_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> app_instance_;
  base::OnceClosure quit_closure_;
  std::string removed_package_;
  const extensions::Extension* installed_extension_ = nullptr;
  const extensions::Extension* uninstalled_extension_ = nullptr;
  extensions::UninstallReason reason_ =
      extensions::UNINSTALL_REASON_FOR_TESTING;
  base::Optional<bool> is_update_;
};

class ApkWebAppInstallerDelayedArcStartBrowserTest
    : public ApkWebAppInstallerBrowserTest {
  // Don't start ARC.
  void SetUpOnMainThread() override {}

  // Don't tear down ARC.
  void TearDownOnMainThread() override {}
};

// Test the full installation and uninstallation flow.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, InstallAndUninstall) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = ApkWebAppService::Get(browser()->profile());
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  app_instance_->SendPackageAdded(GetPackage(kPackageName, kAppTitle));

  WaitForQuit();

  EXPECT_TRUE(installed_extension_);
  EXPECT_EQ(kAppTitle, installed_extension_->name());
  EXPECT_FALSE(is_update_.value());

  // Now send an uninstallation call from ARC, which should uninstall the
  // installed extension. Uninstallation should be synchronous so no need to
  // WaitForQuit().
  app_instance_->SendPackageUninstalled(kPackageName);
  EXPECT_TRUE(uninstalled_extension_);
  EXPECT_EQ(kAppTitle, uninstalled_extension_->name());
  EXPECT_EQ(extensions::UNINSTALL_REASON_ARC, reason_);
}

// Test installation via PackageListRefreshed.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerBrowserTest, PackageListRefreshed) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = ApkWebAppService::Get(browser()->profile());
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetPackage(kPackageName, kAppTitle));
  app_instance_->SendRefreshPackageList(std::move(packages));

  WaitForQuit();

  EXPECT_TRUE(installed_extension_);
  EXPECT_EQ(kAppTitle, installed_extension_->name());
  EXPECT_FALSE(is_update_.value());
}

// Test uninstallation when ARC isn't running.
IN_PROC_BROWSER_TEST_F(ApkWebAppInstallerDelayedArcStartBrowserTest,
                       DelayedUninstall) {
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      observer(this);
  observer.Add(extensions::ExtensionRegistry::Get(browser()->profile()));
  ApkWebAppService* service = ApkWebAppService::Get(browser()->profile());

  // Install an app from the raw data as if ARC had installed it.
  service->OnDidGetWebAppIcon(kPackageName, GetWebAppInfo(kAppTitle),
                              GetFakeIconBytes());

  WaitForQuit();
  EXPECT_TRUE(installed_extension_);
  EXPECT_EQ(kAppTitle, installed_extension_->name());
  EXPECT_FALSE(is_update_.value());

  // Uninstall the app on the extensions side. ARC uninstallation should be
  // queued.
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->UninstallExtension(installed_extension_->id(),
                           extensions::UNINSTALL_REASON_USER_INITIATED,
                           /*error=*/nullptr);
  EXPECT_EQ(extensions::UNINSTALL_REASON_USER_INITIATED, reason_);

  // Start up ARC and set the package to be installed.
  EnableArc();
  app_instance_->SendPackageAdded(GetPackage(kPackageName, kAppTitle));

  // Trigger a package refresh, which should call to ARC to remove the package.
  arc_app_list_prefs_->AddObserver(this);
  service->SetArcAppListPrefsForTesting(arc_app_list_prefs_);
  std::vector<arc::mojom::ArcPackageInfoPtr> packages;
  packages.push_back(GetPackage(kPackageName, kAppTitle));
  app_instance_->SendRefreshPackageList(std::move(packages));

  EXPECT_EQ(kPackageName, removed_package_);

  arc_app_list_prefs_->RemoveObserver(this);
  DisableArc();
}

}  // namespace chromeos
