// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_next_home/app_controller_service.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/test/bind_test_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "chrome/services/app_service/public/cpp/app_update.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace kiosk_next_home {
namespace {

// Fake activity that we use when seeding data to ARC.
constexpr char kFakeActivity[] = "test.kiosk_next_home.activity";

using apps::mojom::AppType;
using apps::mojom::OptionalBool;
using apps::mojom::Readiness;

typedef std::map<std::string, mojom::AppPtr> AppMap;

}  // namespace

class AppControllerServiceTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    arc_test_.SetUp(profile());
    proxy_ = apps::AppServiceProxy::Get(profile());

    app_controller_service_ = AppControllerService::Get(profile());
  }

  void TearDown() override { arc_test_.TearDown(); }

  Profile* profile() { return profile_.get(); }

  std::string GetAppIdFromAndroidPackage(const std::string& package) {
    return ArcAppListPrefs::GetAppId(package, kFakeActivity);
  }

  void AddAndroidPackageToArc(const std::string& package) {
    arc::mojom::AppInfo app_info;
    app_info.package_name = package;

    // We are only interested in the package name that we already set above,
    // but we need to send a full struct so ARC doesn't drop it.
    app_info.name = "test_app_name";
    app_info.activity = kFakeActivity;
    app_info.sticky = false;
    app_info.notifications_enabled = false;
    arc_test_.app_instance()->SendAppAdded(app_info);
  }

  void AddAppDeltaToAppService(apps::mojom::AppPtr delta) {
    std::vector<apps::mojom::AppPtr> deltas;
    deltas.push_back(std::move(delta));
    proxy_->AppRegistryCache().OnApps(std::move(deltas));
  }

  // Gets all apps from the AppControllerService instance being tested and
  // returns them in a map keyed by their |app_id|.
  AppMap GetAppsFromController() {
    AppMap apps;
    app_controller_service_->GetApps(base::BindLambdaForTesting(
        [&apps](std::vector<mojom::AppPtr> app_list) {
          for (const auto& app : app_list) {
            apps[app->app_id] = app.Clone();
          }
        }));
    return apps;
  }

  // Expects the given apps to be returned by a call to
  // AppControllerService::GetApps(). This function doesn't take into account
  // the order of the returned apps.
  void ExpectApps(const std::vector<mojom::App>& expected_apps) {
    AppMap returned_apps_map = GetAppsFromController();

    EXPECT_EQ(expected_apps.size(), returned_apps_map.size())
        << "AppServiceController::GetApps() returned wrong number of apps.";

    for (const auto& expected_app : expected_apps) {
      auto returned_app_it = returned_apps_map.find(expected_app.app_id);
      ASSERT_NE(returned_app_it, returned_apps_map.end())
          << "App with app_id " << expected_app.app_id
          << " was not returned by the AppControllerService::GetApps() "
             "call.";

      mojom::AppPtr& returned_app = returned_app_it->second;

      // Test equality of every single field to make tests failures more
      // readable.
      EXPECT_EQ(returned_app->app_id, expected_app.app_id);
      EXPECT_EQ(returned_app->type, expected_app.type);
      EXPECT_EQ(returned_app->display_name, expected_app.display_name);
      EXPECT_EQ(returned_app->readiness, expected_app.readiness);
      EXPECT_EQ(returned_app->android_package_name,
                expected_app.android_package_name);

      // Catch all clause of equality. This will only be necessary if we add
      // more fields that are not expected above.
      EXPECT_TRUE(expected_app.Equals(*returned_app));
    }
  }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  ArcAppTest arc_test_;
  apps::AppServiceProxy* proxy_;
  AppControllerService* app_controller_service_;
};

TEST_F(AppControllerServiceTest, AppIsFetchedCorrectly) {
  std::string app_id = "fake_app_id";
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kExtension;
  Readiness readiness = Readiness::kReady;

  // Seeding data.
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.android_package_name = "";
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, AndroidAppIsFetchedCorrectly) {
  std::string android_package_name = "fake.app.package";
  std::string app_id = GetAppIdFromAndroidPackage(android_package_name);
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kArc;
  Readiness readiness = Readiness::kReady;

  // Seeding data.
  AddAndroidPackageToArc(android_package_name);
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.android_package_name = android_package_name;
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, AndroidAppWithMissingPackageFetchedCorrectly) {
  std::string android_package_name = "fake.app.package";
  std::string app_id = GetAppIdFromAndroidPackage(android_package_name);
  std::string display_name = "Fake app name";
  AppType app_type = AppType::kArc;
  Readiness readiness = Readiness::kReady;

  // Seeding data. This time we intentionally don't seed the package to ARC.
  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.name = display_name;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  // Since we don't seed information to ARC, we don't expect to receive a
  // package here. In this case, expect an empty string (and not a crash :)
  expected_app.android_package_name = "";
  expected_app.app_id = app_id;
  expected_app.display_name = display_name;
  expected_app.type = app_type;
  expected_app.readiness = readiness;

  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, AppReadinessIsUpdated) {
  std::string app_id = "fake_app_id";
  AppType app_type = AppType::kExtension;
  Readiness readiness = Readiness::kUnknown;

  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.app_type = app_type;
  delta.readiness = readiness;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.app_id = app_id;
  expected_app.type = app_type;
  expected_app.readiness = readiness;
  ExpectApps({expected_app});

  // Now we change the readiness.
  delta.readiness = Readiness::kReady;
  AddAppDeltaToAppService(delta.Clone());

  expected_app.readiness = Readiness::kReady;
  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, AppDisplayNameIsUpdated) {
  std::string app_id = "fake_app_id";
  AppType app_type = AppType::kExtension;
  std::string display_name = "Initial App Name";

  apps::mojom::App delta;
  delta.app_id = app_id;
  delta.app_type = app_type;
  delta.name = display_name;
  delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(delta.Clone());

  mojom::App expected_app;
  expected_app.app_id = app_id;
  expected_app.type = app_type;
  expected_app.display_name = display_name;
  ExpectApps({expected_app});

  // Now we change the name.
  std::string new_display_name = "New App Name";
  delta.name = new_display_name;
  AddAppDeltaToAppService(delta.Clone());

  expected_app.display_name = new_display_name;
  ExpectApps({expected_app});
}

TEST_F(AppControllerServiceTest, MultipleAppsAreFetchedCorrectly) {
  // Seed the first app.
  apps::mojom::App first_delta;
  first_delta.app_id = "first_app";
  first_delta.app_type = AppType::kBuiltIn;
  first_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(first_delta.Clone());

  mojom::App first_expected_app;
  first_expected_app.app_id = "first_app";
  first_expected_app.type = AppType::kBuiltIn;
  // Expect only the first app.
  ExpectApps({first_expected_app});

  // Seed second app.
  apps::mojom::App second_delta;
  second_delta.app_id = "second_app";
  second_delta.app_type = AppType::kBuiltIn;
  second_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(second_delta.Clone());

  mojom::App second_expected_app;
  second_expected_app.app_id = "second_app";
  second_expected_app.type = AppType::kBuiltIn;
  // Expect both apps.
  ExpectApps({first_expected_app, second_expected_app});
}

TEST_F(AppControllerServiceTest, AppsThatAreNotRelevantAreFiltered) {
  // Seed an app that's allowed to be returned by the AppControllerService.
  apps::mojom::App allowed_app_delta;
  allowed_app_delta.app_id = "allowed_app";
  allowed_app_delta.app_type = AppType::kBuiltIn;
  allowed_app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(allowed_app_delta.Clone());

  apps::mojom::App first_blocked_app_delta;
  first_blocked_app_delta.app_id = "first_blocked_app";
  first_blocked_app_delta.app_type = AppType::kBuiltIn;
  first_blocked_app_delta.show_in_launcher = OptionalBool::kUnknown;
  AddAppDeltaToAppService(first_blocked_app_delta.Clone());

  apps::mojom::App second_blocked_app_delta;
  second_blocked_app_delta.app_id = "second_blocked_app";
  second_blocked_app_delta.app_type = AppType::kBuiltIn;
  second_blocked_app_delta.show_in_launcher = OptionalBool::kFalse;
  AddAppDeltaToAppService(second_blocked_app_delta.Clone());

  apps::mojom::App kiosk_next_app_delta;
  kiosk_next_app_delta.app_id = extension_misc::kKioskNextHomeAppId;
  kiosk_next_app_delta.app_type = AppType::kBuiltIn;
  // Even though the Kiosk Next might be allowed in the launcher, we cannot
  // return it.
  kiosk_next_app_delta.show_in_launcher = OptionalBool::kTrue;
  AddAppDeltaToAppService(kiosk_next_app_delta.Clone());

  mojom::App allowed_app;
  allowed_app.app_id = "allowed_app";
  allowed_app.type = AppType::kBuiltIn;

  // Expect only the allowed app, all the other ones were filtered.
  ExpectApps({allowed_app});
}

}  // namespace kiosk_next_home
}  // namespace chromeos
