// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shelf/shelf_util.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_app_deferred_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/common/common_type_converters.h"

namespace mojo {

template <>
struct TypeConverter<arc::mojom::AppInfoPtr, arc::mojom::AppInfo> {
  static arc::mojom::AppInfoPtr Convert(const arc::mojom::AppInfo& app_info) {
    return app_info.Clone();
  }
};

template <>
struct TypeConverter<arc::mojom::ArcPackageInfoPtr,
                     arc::mojom::ArcPackageInfo> {
  static arc::mojom::ArcPackageInfoPtr Convert(
      const arc::mojom::ArcPackageInfo& package_info) {
    return package_info.Clone();
  }
};

}  // namespace mojo

namespace {

const char kTestAppName[] = "Test Arc App";
const char kTestAppPackage[] = "test.arc.app.package";
const char kTestAppActivity[] = "test.arc.app.package.activity";
constexpr int kAppAnimatedThresholdMs = 100;

std::string GetTestAppId() {
  return ArcAppListPrefs::GetAppId(kTestAppPackage, kTestAppActivity);
}

ChromeLauncherController* chrome_controller() {
  return ChromeLauncherController::instance();
}

ash::ShelfDelegate* shelf_delegate() {
  return ash::WmShell::Get()->shelf_delegate();
}

class AppAnimatedWaiter {
 public:
  explicit AppAnimatedWaiter(const std::string& app_id) : app_id_(app_id) {}

  void Wait() {
    const base::TimeDelta threshold =
        base::TimeDelta::FromMilliseconds(kAppAnimatedThresholdMs);
    ArcAppDeferredLauncherController* controller =
        chrome_controller()->GetArcDeferredLauncher();
    while (controller->GetActiveTime(app_id_) < threshold) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  const std::string app_id_;
};

enum TestAction {
  TEST_ACTION_START,  // Start app on app appears.
  TEST_ACTION_EXIT,   // Exit Chrome during animation.
  TEST_ACTION_CLOSE,  // Close item during animation.
};

// Test parameters include TestAction and pin/unpin state.
typedef std::tr1::tuple<TestAction, bool> TestParameter;

TestParameter build_test_parameter[] = {
    TestParameter(TEST_ACTION_START, false),
    TestParameter(TEST_ACTION_EXIT, false),
    TestParameter(TEST_ACTION_CLOSE, false),
    TestParameter(TEST_ACTION_START, true),
};

}  // namespace

class ArcAppLauncherBrowserTest
    : public ExtensionBrowserTest,
      public testing::WithParamInterface<TestParameter> {
 public:
  ArcAppLauncherBrowserTest() {}
  ~ArcAppLauncherBrowserTest() override {}

 protected:
  // content::BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableArc);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcAuthService::DisableUIForTesting();
  }

  void SetUpOnMainThread() override { arc::ArcAuthService::Get()->EnableArc(); }

  void InstallTestApp() {
    std::vector<arc::mojom::AppInfo> apps;

    arc::mojom::AppInfo app;
    app.name = kTestAppName;
    app.package_name = kTestAppPackage;
    app.activity = kTestAppActivity;
    app.sticky = false;
    apps.push_back(app);

    app_host()->OnAppListRefreshed(
        mojo::Array<arc::mojom::AppInfoPtr>::From(apps));

    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
        app_prefs()->GetApp(GetTestAppId());
    ASSERT_TRUE(app_info);
    EXPECT_TRUE(app_info->ready);
    base::RunLoop().RunUntilIdle();
  }

  void StartInstance() {
    auth_service()->OnPrimaryUserProfilePrepared(profile());
    app_instance_observer()->OnInstanceReady();
  }

  void StopInstance() {
    auth_service()->Shutdown();
    app_instance_observer()->OnInstanceClosed();
  }

  ArcAppListPrefs* app_prefs() { return ArcAppListPrefs::Get(profile()); }

  // Returns as AppHost interface in order to access to private implementation
  // of the interface.
  arc::mojom::AppHost* app_host() { return app_prefs(); }

  // Returns as AppInstance observer interface in order to access to private
  // implementation of the interface.
  arc::InstanceHolder<arc::mojom::AppInstance>::Observer*
  app_instance_observer() {
    return app_prefs();
  }

  arc::ArcAuthService* auth_service() { return arc::ArcAuthService::Get(); }

  bool is_pinned() const { return std::tr1::get<1>(GetParam()); }

  TestAction test_action() const { return std::tr1::get<0>(GetParam()); }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppLauncherBrowserTest);
};

// This tests simulates normal workflow for starting Arc app in deferred mode.
IN_PROC_BROWSER_TEST_P(ArcAppLauncherBrowserTest, StartAppDeferred) {
  // Install app to remember existing apps.
  InstallTestApp();

  const std::string app_id = GetTestAppId();
  if (is_pinned()) {
    shelf_delegate()->PinAppWithID(app_id);
    EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id));
  } else {
    EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id));
  }

  StopInstance();
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      app_prefs()->GetApp(app_id);
  EXPECT_FALSE(app_info);

  // Restart instance. App should be taken from prefs but its state is non-ready
  // currently.
  StartInstance();
  app_info = app_prefs()->GetApp(app_id);
  ASSERT_TRUE(app_info);
  EXPECT_FALSE(app_info->ready);
  if (is_pinned())
    EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id));
  else
    EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id));

  // Launching non-ready Arc app creates item on shelf and spinning animation.
  arc::LaunchApp(profile(), app_id);
  EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id));
  AppAnimatedWaiter(app_id).Wait();

  switch (test_action()) {
    case TEST_ACTION_START:
      // Now simulates that Arc is started and app list is refreshed. This
      // should stop animation and delete icon from the shelf.
      InstallTestApp();
      EXPECT_TRUE(chrome_controller()
                      ->GetArcDeferredLauncher()
                      ->GetActiveTime(app_id)
                      .is_zero());
      if (is_pinned())
        EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id));
      else
        EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id));
      break;
    case TEST_ACTION_EXIT:
      // Just exist Chrome.
      break;
    case TEST_ACTION_CLOSE:
      // Close item during animation.
      {
        LauncherItemController* controller =
            chrome_controller()->GetLauncherItemController(
                shelf_delegate()->GetShelfIDForAppID(app_id));
        ASSERT_TRUE(controller);
        controller->Close();
        EXPECT_TRUE(chrome_controller()
                        ->GetArcDeferredLauncher()
                        ->GetActiveTime(app_id)
                        .is_zero());
        if (is_pinned())
          EXPECT_TRUE(shelf_delegate()->GetShelfIDForAppID(app_id));
        else
          EXPECT_FALSE(shelf_delegate()->GetShelfIDForAppID(app_id));
      }
      break;
  }
}

INSTANTIATE_TEST_CASE_P(ArcAppLauncherBrowserTestInstance,
                        ArcAppLauncherBrowserTest,
                        ::testing::ValuesIn(build_test_parameter));
