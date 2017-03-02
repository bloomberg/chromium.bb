// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_auth_notification.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/arc/arc_util.h"
#include "components/arc/common/app.mojom.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/test_utils.h"

namespace arc {

class ArcAppUninstallDialogViewBrowserTest : public InProcessBrowserTest {
 public:
  ArcAppUninstallDialogViewBrowserTest() {}

  // InProcessBrowserTest:
  ~ArcAppUninstallDialogViewBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    ArcSessionManager::DisableUIForTesting();
    ArcAuthNotification::DisableForTesting();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    profile_ = browser()->profile();
    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    if (!arc_app_list_pref_) {
      ArcAppListPrefsFactory::GetInstance()->RecreateServiceInstanceForTesting(
          profile_);
    }

    arc::SetArcPlayStoreEnabledForProfile(profile_, true);

    arc_app_list_pref_ = ArcAppListPrefs::Get(profile_);
    DCHECK(arc_app_list_pref_);

    base::RunLoop run_loop;
    arc_app_list_pref_->SetDefaltAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    app_instance_.reset(new arc::FakeAppInstance(arc_app_list_pref_));
    arc_app_list_pref_->app_instance_holder()->SetInstance(app_instance_.get());

    // In this setup, we have one app and one shortcut which share one package.
    mojom::AppInfo app;
    app.name = base::StringPrintf("Fake App %d", 0);
    app.package_name = base::StringPrintf("fake.package.%d", 0);
    app.activity = base::StringPrintf("fake.app.%d.activity", 0);
    app.sticky = false;
    app_instance_->SendRefreshAppList(std::vector<mojom::AppInfo>(1, app));

    mojom::ShortcutInfo shortcut;
    shortcut.name = base::StringPrintf("Fake Shortcut %d", 0);
    shortcut.package_name = base::StringPrintf("fake.package.%d", 0);
    shortcut.intent_uri = base::StringPrintf("Fake Shortcut uri %d", 0);
    app_instance_->SendInstallShortcut(shortcut);

    mojom::ArcPackageInfo package;
    package.package_name = base::StringPrintf("fake.package.%d", 0);
    package.package_version = 0;
    package.last_backup_android_id = 0;
    package.last_backup_time = 0;
    package.sync = false;
    app_instance_->SendRefreshPackageList(
        std::vector<mojom::ArcPackageInfo>(1, package));
  }

  void TearDownOnMainThread() override {
    ArcSessionManager::Get()->Shutdown();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  // Ensures the ArcAppDialogView is destoryed.
  void TearDown() override { ASSERT_FALSE(IsArcAppDialogViewAliveForTest()); }

  ArcAppListPrefs* arc_app_list_pref() { return arc_app_list_pref_; }

  FakeAppInstance* instance() { return app_instance_.get(); }

 private:
  ArcAppListPrefs* arc_app_list_pref_ = nullptr;

  Profile* profile_ = nullptr;

  std::unique_ptr<arc::FakeAppInstance> app_instance_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppUninstallDialogViewBrowserTest);
};

// User confirms/cancels ARC app uninstall. Note that the shortcut is removed
// when the app and the package are uninstalled since the shortcut and the app
// share same package.
IN_PROC_BROWSER_TEST_F(ArcAppUninstallDialogViewBrowserTest,
                       UserConfirmsUninstall) {
  std::vector<std::string> app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 2u);
  std::string package_name = base::StringPrintf("fake.package.%d", 0);
  std::string app_activity = base::StringPrintf("fake.app.%d.activity", 0);
  std::string app_id =
      arc_app_list_pref()->GetAppId(package_name, app_activity);

  AppListService* service = AppListService::Get();
  ASSERT_TRUE(service);
  service->ShowForProfile(browser()->profile());
  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);
  ShowArcAppUninstallDialog(browser()->profile(), controller, app_id);
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(false));
  content::RunAllPendingInMessageLoop();
  app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 2u);

  ShowArcAppUninstallDialog(browser()->profile(), controller, app_id);
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();
  app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 0u);
  controller->DismissView();
}

// User confirms/cancels ARC app shortcut removal. Note that the app is not
// uninstalled when the shortcut is removed.
IN_PROC_BROWSER_TEST_F(ArcAppUninstallDialogViewBrowserTest,
                       UserConfirmsUninstallShortcut) {
  std::vector<std::string> app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 2u);
  std::string package_name = base::StringPrintf("fake.package.%d", 0);
  std::string intent_uri = base::StringPrintf("Fake Shortcut uri %d", 0);
  std::string app_id = arc_app_list_pref()->GetAppId(package_name, intent_uri);

  AppListService* service = AppListService::Get();
  ASSERT_TRUE(service);
  service->ShowForProfile(browser()->profile());
  AppListControllerDelegate* controller(service->GetControllerDelegate());
  ASSERT_TRUE(controller);
  ShowArcAppUninstallDialog(browser()->profile(), controller, app_id);
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(false));
  content::RunAllPendingInMessageLoop();
  app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 2u);

  ShowArcAppUninstallDialog(browser()->profile(), controller, app_id);
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(CloseAppDialogViewAndConfirmForTest(true));
  content::RunAllPendingInMessageLoop();
  app_ids = arc_app_list_pref()->GetAppIds();
  EXPECT_EQ(app_ids.size(), 1u);
  controller->DismissView();
}

}  // namespace arc
