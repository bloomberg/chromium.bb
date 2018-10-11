// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/fake_cros_component_manager.h"
#include "chrome/test/base/browser_process_platform_part_test_api_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using component_updater::FakeCrOSComponentManager;

namespace chromeos {

namespace {

// TODO(michaelpg): Clean up tests for offline resources and differentiate
// between the CrOS component and the preinstalled resources image.
constexpr char kOfflineResourcesComponent[] = "demo-mode-resources";
constexpr char kTestDemoModeResourcesMountPoint[] =
    "/run/imageloader/demo_mode_resources";
constexpr char kDemoAppsImageFile[] = "android_demo_apps.squash";
constexpr char kExternalExtensionsPrefsFile[] = "demo_extensions.json";

void SetBoolean(bool* value) {
  *value = true;
}

}  // namespace

class DemoSessionTest : public testing::Test {
 public:
  DemoSessionTest()
      : browser_process_platform_part_test_api_(
            g_browser_process->platform_part()) {}
  ~DemoSessionTest() override = default;

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    InitializeCrosComponentManager();
    session_manager_ = std::make_unique<session_manager::SessionManager>();
  }

  void TearDown() override {
    DemoSession::ShutDownIfInitialized();
    DemoSession::ResetDemoConfigForTesting();

    chromeos::DBusThreadManager::Shutdown();

    cros_component_manager_ = nullptr;
    browser_process_platform_part_test_api_.ShutdownCrosComponentManager();
  }

 protected:
  bool FinishResourcesComponentLoad(const base::FilePath& mount_path) {
    EXPECT_TRUE(
        cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
    EXPECT_FALSE(
        cros_component_manager_->UpdateRequested(kOfflineResourcesComponent));

    return cros_component_manager_->FinishLoadRequest(
        kOfflineResourcesComponent,
        FakeCrOSComponentManager::ComponentInfo(
            component_updater::CrOSComponentManager::Error::NONE,
            base::FilePath("/dev/null"), mount_path));
  }

  void InitializeCrosComponentManager() {
    auto fake_cros_component_manager =
        std::make_unique<FakeCrOSComponentManager>();
    fake_cros_component_manager->set_queue_load_requests(true);
    fake_cros_component_manager->set_supported_components(
        {kOfflineResourcesComponent});
    cros_component_manager_ = fake_cros_component_manager.get();

    browser_process_platform_part_test_api_.InitializeCrosComponentManager(
        std::move(fake_cros_component_manager));
  }

  FakeCrOSComponentManager* cros_component_manager_ = nullptr;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<session_manager::SessionManager> session_manager_;

 private:
  BrowserProcessPlatformPartTestApi browser_process_platform_part_test_api_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionTest);
};

TEST_F(DemoSessionTest, StartForDeviceInDemoMode) {
  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_FALSE(demo_session->offline_enrolled());
  EXPECT_EQ(demo_session, DemoSession::Get());
}

TEST_F(DemoSessionTest, StartInitiatesOfflineResourcesLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));

  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
  EXPECT_EQ(
      component_mount_point.AppendASCII("foo.txt"),
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo.txt")));
  EXPECT_EQ(component_mount_point.AppendASCII("foo/bar.txt"),
            demo_session->GetOfflineResourceAbsolutePath(
                base::FilePath("foo/bar.txt")));
  EXPECT_EQ(
      component_mount_point.AppendASCII("foo/"),
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo/")));
  EXPECT_TRUE(
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("../foo/"))
          .empty());
  EXPECT_TRUE(
      demo_session->GetOfflineResourceAbsolutePath(base::FilePath("foo/../bar"))
          .empty());
}

TEST_F(DemoSessionTest, StartForDemoDeviceNotInDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(DemoSession::StartIfInDemoMode());
  EXPECT_FALSE(DemoSession::Get());

  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, StartIfInOfflineEnrolledDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOffline);

  EXPECT_FALSE(DemoSession::Get());
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_enrolled());
  EXPECT_EQ(demo_session, DemoSession::Get());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfInDemoMode) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);
  EXPECT_FALSE(demo_session->started());
  EXPECT_FALSE(demo_session->offline_enrolled());

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_FALSE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfNotInDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kNone);
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_FALSE(DemoSession::Get());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, PreloadOfflineResourcesIfInOfflineDemoMode) {
  DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOffline);
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);
  EXPECT_FALSE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_enrolled());

  EXPECT_FALSE(demo_session->offline_resources_loaded());
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, ShutdownResetsInstance) {
  ASSERT_TRUE(DemoSession::StartIfInDemoMode());
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, ShutdownAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  EXPECT_TRUE(DemoSession::Get());
  DemoSession::ShutDownIfInitialized();
  EXPECT_FALSE(DemoSession::Get());
}

TEST_F(DemoSessionTest, StartDemoSessionWhilePreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();

  ASSERT_TRUE(demo_session);
  EXPECT_TRUE(demo_session->started());

  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, StartDemoSessionAfterPreloadingResources) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  EXPECT_TRUE(demo_session->started());
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());

  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterStart) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterOfflineResourceLoad) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, EnsureOfflineResourcesLoadedAfterPreload) {
  DemoSession::PreloadOfflineResourcesIfInDemoMode();

  DemoSession* demo_session = DemoSession::Get();
  ASSERT_TRUE(demo_session);

  bool callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &callback_called));

  EXPECT_FALSE(callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
  EXPECT_EQ(component_mount_point.AppendASCII(kExternalExtensionsPrefsFile),
            demo_session->GetExternalExtensionsPrefsPath());
}

TEST_F(DemoSessionTest, MultipleEnsureOfflineResourcesLoaded) {
  DemoSession* demo_session = DemoSession::StartIfInDemoMode();
  ASSERT_TRUE(demo_session);

  bool first_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &first_callback_called));

  bool second_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &second_callback_called));

  bool third_callback_called = false;
  demo_session->EnsureOfflineResourcesLoaded(
      base::BindOnce(&SetBoolean, &third_callback_called));

  EXPECT_FALSE(first_callback_called);
  EXPECT_FALSE(second_callback_called);
  EXPECT_FALSE(third_callback_called);
  EXPECT_FALSE(demo_session->offline_resources_loaded());

  const base::FilePath component_mount_point =
      base::FilePath(kTestDemoModeResourcesMountPoint);
  ASSERT_TRUE(FinishResourcesComponentLoad(component_mount_point));
  EXPECT_FALSE(
      cros_component_manager_->HasPendingInstall(kOfflineResourcesComponent));

  EXPECT_TRUE(first_callback_called);
  EXPECT_TRUE(second_callback_called);
  EXPECT_TRUE(third_callback_called);
  EXPECT_TRUE(demo_session->offline_resources_loaded());
  EXPECT_EQ(component_mount_point.AppendASCII(kDemoAppsImageFile),
            demo_session->GetDemoAppsPath());
}

}  // namespace chromeos
