// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

namespace {

// Helper function to create a fake app instance and wait for the instance to be
// ready.
std::unique_ptr<arc::FakeAppInstance> CreateAppInstance(
    ArcAppListPrefs* prefs) {
  std::unique_ptr<arc::FakeAppInstance> app_instance =
      std::make_unique<arc::FakeAppInstance>(prefs);
  arc::ArcServiceManager::Get()->arc_bridge_service()->app()->SetInstance(
      app_instance.get());
  WaitForInstanceReady(
      arc::ArcServiceManager::Get()->arc_bridge_service()->app());
  return app_instance;
}

}  // namespace

class ArcAppsPrivateApiTest : public extensions::ExtensionApiTest {
 public:
  ArcAppsPrivateApiTest() = default;
  ~ArcAppsPrivateApiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    std::unique_ptr<chromeos::FakeSessionManagerClient> session_manager_client =
        std::make_unique<chromeos::FakeSessionManagerClient>();
    session_manager_client->set_arc_available(true);
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(session_manager_client));
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    arc::SetArcPlayStoreEnabledForProfile(profile(), true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppsPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, GetPackageNameAndLaunchApp) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  std::unique_ptr<arc::FakeAppInstance> app_instance = CreateAppInstance(prefs);
  // Add one launchable app and one non-launchable app.
  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");
  app_instance->SendRefreshAppList({launchable_app});
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      base::nullopt /* intent */);

  // Stopping the service makes the app non-ready.
  arc::ArcServiceManager::Get()->arc_bridge_service()->app()->CloseInstance(
      app_instance.get());
  EXPECT_EQ(0u, app_instance->launch_requests().size());
  // Verify |chrome.arcAppsPrivate.getLaunchableApps| returns the package name
  // of the launchable app only. The JS test will attempt to launch the app.
  EXPECT_TRUE(
      RunPlatformAppTestWithArg("arc_app_launcher/launch_app", "Package_0"))
      << message_;

  // Verify the app is not launched because it's not ready.
  EXPECT_EQ(0u, app_instance->launch_requests().size());
  // Restarting the service makes the app ready. Verify the app is launched
  // successfully.
  app_instance = CreateAppInstance(prefs);
  app_instance->SendRefreshAppList({launchable_app});
  ASSERT_EQ(1u, app_instance->launch_requests().size());
  EXPECT_TRUE(app_instance->launch_requests()[0]->IsForApp(launchable_app));
}

IN_PROC_BROWSER_TEST_F(ArcAppsPrivateApiTest, OnInstalled) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(browser()->profile());
  ASSERT_TRUE(prefs);
  std::unique_ptr<arc::FakeAppInstance> app_instance = CreateAppInstance(prefs);
  arc::mojom::AppInfo launchable_app("App_0", "Package_0", "Dummy_activity_0");

  // The JS test will observe the onInstalled event and attempt to launch the
  // newly installed app.
  SetCustomArg("Package_0");
  extensions::ResultCatcher catcher;
  ExtensionTestMessageListener ready_listener("ready", false);

  base::FilePath path =
      test_data_dir_.AppendASCII("arc_app_launcher/install_event");
  const extensions::Extension* app = LoadExtension(path);
  ASSERT_TRUE(app);
  EXPECT_TRUE(ready_listener.WaitUntilSatisfied());

  EXPECT_EQ(0u, app_instance->launch_requests().size());
  // Add one launchable app and one non-launchable app.
  app_instance->SendRefreshAppList({launchable_app});
  static_cast<arc::mojom::AppHost*>(prefs)->OnTaskCreated(
      0 /* task_id */, "Package_1", "Dummy_activity_1", "App_1",
      base::nullopt /* intent */);
  // Verify the JS test receives the onInstalled event for the launchable app
  // only, and the app is launched successfully.
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  ASSERT_EQ(1u, app_instance->launch_requests().size());
  EXPECT_TRUE(app_instance->launch_requests()[0]->IsForApp(launchable_app));
}