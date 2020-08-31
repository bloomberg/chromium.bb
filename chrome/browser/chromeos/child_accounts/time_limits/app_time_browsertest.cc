// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_policy_builder.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/login/test/scoped_policy_update.h"
#include "chrome/browser/chromeos/login/test/user_policy_mixin.h"
#include "chrome/browser/chromeos/policy/user_policy_test_helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/arc/arc_util.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/test/connection_holder_util.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace app_time {

namespace {

arc::mojom::ArcPackageInfoPtr CreateArcAppPackage(
    const std::string& package_name) {
  auto package = arc::mojom::ArcPackageInfo::New();
  package->package_name = package_name;
  package->package_version = 1;
  package->last_backup_android_id = 1;
  package->last_backup_time = 1;
  package->sync = false;
  package->system = false;
  package->permissions = base::flat_map<::arc::mojom::AppPermission, bool>();
  return package;
}

arc::mojom::AppInfo CreateArcAppInfo(const std::string& package_name) {
  arc::mojom::AppInfo app;
  app.package_name = package_name;
  app.name = package_name;
  app.activity = base::StrCat({package_name, ".", "activity"});
  app.sticky = true;
  return app;
}

}  // namespace

// Integration tests for Per-App Time Limits feature.
class AppTimeTest : public MixinBasedInProcessBrowserTest {
 protected:
  AppTimeTest() = default;
  AppTimeTest(const AppTimeTest&) = delete;
  AppTimeTest& operator=(const AppTimeTest&) = delete;
  ~AppTimeTest() override = default;

  virtual bool ShouldEnableWebTimeLimit() { return true; }

  // MixinBasedInProcessBrowserTest:
  void SetUp() override {
    std::vector<base::Feature> enabled_features{features::kPerAppTimeLimits};
    std::vector<base::Feature> disabled_features;
    if (ShouldEnableWebTimeLimit())
      enabled_features.push_back(features::kWebTimeLimits);
    else
      disabled_features.push_back(features::kWebTimeLimits);

    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    arc::SetArcAvailableCommandLineForTesting(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
  }

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Started());
    host_resolver()->AddIPLiteralRule("*", "127.0.0.1", "localhost");
    logged_in_user_mixin_.LogInUser(false /*issue_any_scope_token*/,
                                    true /*wait_for_active_session*/,
                                    true /*request_policy_update*/);

    arc::SetArcPlayStoreEnabledForProfile(browser()->profile(), true);
    arc_app_list_prefs_ = ArcAppListPrefs::Get(browser()->profile());
    EXPECT_TRUE(arc_app_list_prefs_);

    base::RunLoop run_loop;
    arc_app_list_prefs_->SetDefaultAppsReadyCallback(run_loop.QuitClosure());
    run_loop.Run();

    arc_app_instance_ =
        std::make_unique<arc::FakeAppInstance>(arc_app_list_prefs_);
    arc_app_list_prefs_->app_connection_holder()->SetInstance(
        arc_app_instance_.get());
    WaitForInstanceReady(arc_app_list_prefs_->app_connection_holder());
    arc_app_instance_->set_icon_response_type(
        arc::FakeAppInstance::IconResponseType::ICON_RESPONSE_SKIP);
  }

  void TearDownOnMainThread() override {
    arc_app_list_prefs_->app_connection_holder()->CloseInstance(
        arc_app_instance_.get());
    arc_app_instance_.reset();
    arc::ArcSessionManager::Get()->Shutdown();
  }

  void UpdatePerAppTimeLimitsPolicy(const base::Value& policy) {
    std::string policy_value;
    base::JSONWriter::Write(policy, &policy_value);

    logged_in_user_mixin_.GetUserPolicyMixin()
        ->RequestPolicyUpdate()
        ->policy_payload()
        ->mutable_perapptimelimits()
        ->set_value(policy_value);

    logged_in_user_mixin_.GetUserPolicyTestHelper()->RefreshPolicyAndWait(
        GetCurrentProfile());

    base::RunLoop().RunUntilIdle();
  }

  AppActivityRegistry* GetAppRegistry() {
    auto child_user_service = ChildUserService::TestApi(
        ChildUserServiceFactory::GetForBrowserContext(GetCurrentProfile()));
    EXPECT_TRUE(child_user_service.app_time_controller());
    auto app_time_controller =
        AppTimeController::TestApi(child_user_service.app_time_controller());
    return app_time_controller.app_registry();
  }

  void InstallArcApp(const AppId& app_id) {
    EXPECT_EQ(apps::mojom::AppType::kArc, app_id.app_type());
    const std::string& package_name = app_id.app_id();
    arc_app_instance_->SendPackageAdded(
        CreateArcAppPackage(package_name)->Clone());

    const arc::mojom::AppInfo app = CreateArcAppInfo(package_name);
    arc_app_instance_->SendPackageAppListRefreshed(package_name, {app});

    base::RunLoop().RunUntilIdle();
  }

  void Equals(const AppLimit& limit1, const AppLimit& limit2) {
    EXPECT_EQ(limit1.restriction(), limit2.restriction());
    EXPECT_EQ(limit1.daily_limit(), limit2.daily_limit());
    // Compare JavaTime, because some precision is lost when serializing
    // and deserializing.
    EXPECT_EQ(limit1.last_updated().ToJavaTime(),
              limit2.last_updated().ToJavaTime());
  }

 private:
  Profile* GetCurrentProfile() {
    const user_manager::UserManager* const user_manager =
        user_manager::UserManager::Get();
    Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(
        user_manager->GetActiveUser());
    EXPECT_TRUE(profile);

    return profile;
  }

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kChild,
      embedded_test_server(), this};

  ArcAppListPrefs* arc_app_list_prefs_ = nullptr;
  std::unique_ptr<arc::FakeAppInstance> arc_app_instance_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppTimeTest, AppInstallation) {
  const AppId app1(apps::mojom::AppType::kArc, "com.example.app1");
  AppActivityRegistry* app_registry = GetAppRegistry();
  EXPECT_FALSE(app_registry->IsAppInstalled(app1));

  InstallArcApp(app1);

  ASSERT_TRUE(app_registry->IsAppInstalled(app1));
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
}

IN_PROC_BROWSER_TEST_F(AppTimeTest, PerAppTimeLimitsPolicyUpdates) {
  // Install an app.
  const AppId app1(apps::mojom::AppType::kArc, "com.example.app1");
  InstallArcApp(app1);

  AppActivityRegistry* app_registry = GetAppRegistry();
  AppActivityRegistry::TestApi app_registry_test(app_registry);
  ASSERT_TRUE(app_registry->IsAppInstalled(app1));
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));

  // Block the app.
  AppTimeLimitsPolicyBuilder block_policy;
  const AppLimit block_limit =
      AppLimit(AppRestriction::kBlocked, base::nullopt, base::Time::Now());
  block_policy.AddAppLimit(app1, block_limit);
  block_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(block_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(block_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit for the app - app should not paused.
  AppTimeLimitsPolicyBuilder time_limit_policy;
  const AppLimit time_limit =
      AppLimit(AppRestriction::kTimeLimit, base::TimeDelta::FromHours(1),
               base::Time::Now());
  time_limit_policy.AddAppLimit(app1, time_limit);
  time_limit_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(time_limit_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(time_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit of zero - app should be paused.
  AppTimeLimitsPolicyBuilder zero_time_limit_policy;
  const AppLimit zero_limit =
      AppLimit(AppRestriction::kTimeLimit, base::TimeDelta::FromHours(0),
               base::Time::Now());
  zero_time_limit_policy.AddAppLimit(app1, zero_limit);
  zero_time_limit_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(zero_time_limit_policy.value());
  EXPECT_FALSE(app_registry->IsAppAvailable(app1));
  EXPECT_TRUE(app_registry->IsAppTimeLimitReached(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(zero_limit, app_registry_test.GetAppLimit(app1).value());

  // Set time limit grater then zero again - app should be available again.
  UpdatePerAppTimeLimitsPolicy(time_limit_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry->IsAppTimeLimitReached(app1));
  ASSERT_TRUE(app_registry_test.GetAppLimit(app1));
  Equals(time_limit, app_registry_test.GetAppLimit(app1).value());

  // Remove app restrictions.
  AppTimeLimitsPolicyBuilder no_limits_policy;
  no_limits_policy.SetResetTime(6, 0);

  UpdatePerAppTimeLimitsPolicy(no_limits_policy.value());
  EXPECT_TRUE(app_registry->IsAppAvailable(app1));
  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));
}

IN_PROC_BROWSER_TEST_F(AppTimeTest, PerAppTimeLimitsPolicyMultipleEntries) {
  // Install apps.
  const AppId app1(apps::mojom::AppType::kArc, "com.example.app1");
  InstallArcApp(app1);
  const AppId app2(apps::mojom::AppType::kArc, "com.example.app2");
  InstallArcApp(app2);
  const AppId app3(apps::mojom::AppType::kArc, "com.example.app3");
  InstallArcApp(app3);
  const AppId app4(apps::mojom::AppType::kArc, "com.example.app4");
  InstallArcApp(app4);

  AppActivityRegistry* app_registry = GetAppRegistry();
  AppActivityRegistry::TestApi app_registry_test(app_registry);
  for (const auto& app : {app1, app2, app3, app4}) {
    ASSERT_TRUE(app_registry->IsAppInstalled(app));
    EXPECT_TRUE(app_registry->IsAppAvailable(app));
    EXPECT_FALSE(app_registry_test.GetAppLimit(app));
  }

  // Send policy.
  AppTimeLimitsPolicyBuilder policy;
  policy.SetResetTime(6, 0);
  policy.AddAppLimit(app2, AppLimit(AppRestriction::kBlocked, base::nullopt,
                                    base::Time::Now()));
  policy.AddAppLimit(
      app3, AppLimit(AppRestriction::kTimeLimit,
                     base::TimeDelta::FromMinutes(15), base::Time::Now()));
  policy.AddAppLimit(
      app4, AppLimit(AppRestriction::kTimeLimit, base::TimeDelta::FromHours(1),
                     base::Time::Now()));

  UpdatePerAppTimeLimitsPolicy(policy.value());

  EXPECT_FALSE(app_registry_test.GetAppLimit(app1));

  ASSERT_TRUE(app_registry_test.GetAppLimit(app2));
  EXPECT_EQ(AppRestriction::kBlocked,
            app_registry_test.GetAppLimit(app2)->restriction());

  ASSERT_TRUE(app_registry_test.GetAppLimit(app3));
  EXPECT_EQ(AppRestriction::kTimeLimit,
            app_registry_test.GetAppLimit(app3)->restriction());

  ASSERT_TRUE(app_registry_test.GetAppLimit(app4));
  EXPECT_EQ(AppRestriction::kTimeLimit,
            app_registry_test.GetAppLimit(app4)->restriction());
}

class WebTimeLimitDisabledTest : public AppTimeTest {
 protected:
  WebTimeLimitDisabledTest() = default;
  WebTimeLimitDisabledTest(const WebTimeLimitDisabledTest&) = delete;
  WebTimeLimitDisabledTest& operator=(const WebTimeLimitDisabledTest&) = delete;
  ~WebTimeLimitDisabledTest() override = default;

  bool ShouldEnableWebTimeLimit() override { return false; }
};

IN_PROC_BROWSER_TEST_F(WebTimeLimitDisabledTest, WebTimeLimitDisabled) {
  AppTimeLimitsPolicyBuilder policy;
  policy.SetResetTime(6, 0);
  policy.AddAppLimit(GetChromeAppId(), AppLimit(AppRestriction::kTimeLimit,
                                                base::TimeDelta::FromMinutes(0),
                                                base::Time::Now()));

  UpdatePerAppTimeLimitsPolicy(policy.value());

  AppActivityRegistry* app_registry = GetAppRegistry();
  AppActivityRegistry::TestApi app_registry_test(app_registry);
  EXPECT_FALSE(app_registry_test.GetAppLimit(GetChromeAppId()));
}

}  // namespace app_time
}  // namespace chromeos
