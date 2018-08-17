// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/web_applications/bookmark_apps/policy/web_app_policy_constants.h"
#include "chrome/browser/web_applications/components/pending_app_manager.h"
#include "chrome/browser/web_applications/components/test_pending_app_manager.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using sync_preferences::TestingPrefServiceSyncable;

namespace web_app {

namespace {

const char kUrl1[] = "https://mobile.twitter.com";
const char kUrl2[] = "https://www.google.com";

}  // namespace

class WebAppPolicyManagerTest : public testing::Test {
 public:
  WebAppPolicyManagerTest() = default;
  ~WebAppPolicyManagerTest() override = default;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(WebAppPolicyManagerTest);
};


TEST_F(WebAppPolicyManagerTest, NoForceInstalledAppsPrefValue) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->installed_apps();
  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, NoForceInstalledApps) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  prefs->Set(prefs::kWebAppInstallForceList,
             base::Value(base::Value::Type::LIST));

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->installed_apps();
  EXPECT_TRUE(apps_to_install.empty());
}

TEST_F(WebAppPolicyManagerTest, TwoForceInstalledApps) {
  auto prefs = std::make_unique<TestingPrefServiceSyncable>();
  RegisterUserProfilePrefs(prefs->registry());

  base::Value list(base::Value::Type::LIST);
  // Add two sites, one that opens in a window and one that opens in a tab.
  {
    base::Value item1(base::Value::Type::DICTIONARY);
    item1.SetKey(kUrlKey, base::Value(kUrl1));
    item1.SetKey(kLaunchContainerKey, base::Value(kLaunchContainerWindowValue));

    base::Value item2(base::Value::Type::DICTIONARY);
    item2.SetKey(kUrlKey, base::Value(kUrl2));
    item2.SetKey(kLaunchContainerKey, base::Value(kLaunchContainerTabValue));

    list.GetList().push_back(std::move(item1));
    list.GetList().push_back(std::move(item2));

    prefs->Set(prefs::kWebAppInstallForceList, std::move(list));
  }

  auto pending_app_manager = std::make_unique<TestPendingAppManager>();
  WebAppPolicyManager web_app_policy_manager(prefs.get(),
                                             pending_app_manager.get());
  base::RunLoop().RunUntilIdle();

  const auto& apps_to_install = pending_app_manager->installed_apps();

  std::vector<PendingAppManager::AppInfo> expected_apps_to_install;
  expected_apps_to_install.emplace_back(
      GURL(kUrl1), PendingAppManager::LaunchContainer::kWindow);
  expected_apps_to_install.emplace_back(
      GURL(kUrl2), PendingAppManager::LaunchContainer::kTab);

  EXPECT_EQ(apps_to_install, expected_apps_to_install);
}

}  // namespace web_app
