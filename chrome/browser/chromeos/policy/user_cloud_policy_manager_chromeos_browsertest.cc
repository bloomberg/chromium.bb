// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/logged_in_user_mixin.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace policy {

class UserCloudPolicyManagerTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<
          std::tuple<std::vector<base::Feature>,
                     chromeos::LoggedInUserMixin::LogInType>> {
 protected:
  UserCloudPolicyManagerTest() {
    // Override default tests configuration that prevents effective testing of
    // whether start-up URL policy is properly applied:
    // *   InProcessBrowserTest force about://blank start-up URL via command
    //     line (which trumps policy values).
    set_open_about_blank_on_browser_launch(false);

    scoped_feature_list_.InitWithFeatures(
        std::get<0>(GetParam()) /* enabled_features */,
        std::vector<base::Feature>() /* disabled_features */);
  }

  ~UserCloudPolicyManagerTest() override = default;

  void TearDown() override {
    policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(nullptr);
    MixinBasedInProcessBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(chromeos::switches::kOobeSkipPostLogin);
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  // Sets up fake GAIA for specified user login, and requests login for the user
  // (using LoggedInUserMixin).
  void StartUserLogIn(bool wait_for_active_session) {
    logged_in_user_mixin_.LogInUser(false /*issue_any_scope_token*/,
                                    wait_for_active_session);
  }

 protected:
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, std::get<1>(GetParam()) /*type*/, embedded_test_server(),
      this, true /*should_launch_browser*/,
      AccountId::FromUserEmailGaiaId(
          chromeos::FakeGaiaMixin::kEnterpriseUser1,
          chromeos::FakeGaiaMixin::kEnterpriseUser1GaiaId),
      // Initializing the login manager with no user will cause GAIA screen to
      // be shown on start-up.
      false /*include_initial_user*/};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyManagerTest);
};

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, StartSession) {
  // User hasn't signed in yet, so shouldn't know if the user requires policy.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // Set up start-up URLs through a mandatory user policy.
  const char* const kStartupURLs[] = {"chrome://policy", "chrome://about"};
  enterprise_management::StringList* startup_urls_proto =
      logged_in_user_mixin_.GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_payload()
          ->mutable_restoreonstartupurls()
          ->mutable_value();
  for (auto* const url : kStartupURLs)
    startup_urls_proto->add_entries(url);
  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_restoreonstartup()
      ->set_value(SessionStartupPref::kPrefValueURLs);

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // Check that the startup pages specified in policy were opened.
  BrowserList* browser_list = BrowserList::GetInstance();
  EXPECT_EQ(1U, browser_list->size());
  Browser* browser = browser_list->get(0);
  ASSERT_TRUE(browser);

  TabStripModel* tabs = browser->tab_strip_model();
  ASSERT_TRUE(tabs);
  const int expected_tab_count = static_cast<int>(base::size(kStartupURLs));
  EXPECT_EQ(expected_tab_count, tabs->count());
  for (int i = 0; i < expected_tab_count && i < tabs->count(); ++i) {
    EXPECT_EQ(GURL(kStartupURLs[i]),
              tabs->GetWebContentsAt(i)->GetVisibleURL());
  }

  // User should be marked as requiring policy.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // It is expected that if ArcEnabled policy is not set then it is managed
  // by default and user is not able manually set it.
  EXPECT_TRUE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest, ErrorLoadingPolicy) {
  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       ErrorLoadingPolicyForUnmanagedUser) {
  // Mark user as not needing policy - errors loading policy should be
  // ignored (unlike previous ErrorLoadingPolicy test).
  user_manager::known_user::SetProfileRequiresPolicy(
      logged_in_user_mixin_.GetAccountId(),
      user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired);

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User should still be marked as not needing policy
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerTest,
                       NoPolicyForNonEnterpriseUser) {
  // Recognize example.com as non-enterprise account. We don't use any
  // available public domain such as gmail.com in order to prevent possible
  // leak of verification keys/signatures.
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));
  // If a user signs in with a known non-enterprise account there should be no
  // policy.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User should be marked as not requiring policy.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kNoPolicyRequired,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

using UserCloudPolicyManagerChildTest = UserCloudPolicyManagerTest;

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerChildTest, PolicyForChildUser) {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));

  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  logged_in_user_mixin_.GetUserPolicyMixin()
      ->RequestPolicyUpdate()
      ->policy_payload()
      ->Clear();
  StartUserLogIn(true /*wait_for_active_session*/);

  // User should be marked as having a valid OAuth token.
  const user_manager::UserManager* const user_manager =
      user_manager::UserManager::Get();
  EXPECT_EQ(user_manager::User::OAUTH2_TOKEN_STATUS_VALID,
            user_manager->GetActiveUser()->oauth_token_status());

  // User of CHILD type should be marked as requiring policy.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kPolicyRequired,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  // It is expected that if ArcEnabled policy is not set then it is not managed
  // by default and user is able manually set it.
  EXPECT_FALSE(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->IsManagedPreference(
          arc::prefs::kArcEnabled));
}

IN_PROC_BROWSER_TEST_P(UserCloudPolicyManagerChildTest,
                       PolicyForChildUserMissing) {
  policy::BrowserPolicyConnector::SetNonEnterpriseDomainForTesting(
      "example.com");
  EXPECT_TRUE(policy::BrowserPolicyConnector::IsNonEnterpriseUser(
      logged_in_user_mixin_.GetAccountId().GetUserEmail()));

  // If a user signs in with a known non-enterprise account there should be no
  // policy in case user type is child.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));

  StartUserLogIn(false /*wait_for_active_session*/);
  RunUntilBrowserProcessQuits();

  // Session should not have been started.
  EXPECT_FALSE(session_manager::SessionManager::Get()->IsSessionStarted());

  // User should be marked as not knowing if policy is required yet.
  EXPECT_EQ(user_manager::known_user::ProfileRequiresPolicy::kUnknown,
            user_manager::known_user::GetProfileRequiresPolicy(
                logged_in_user_mixin_.GetAccountId()));
}

const std::vector<base::Feature> feature_lists[] = {
    {},
    {features::kDMServerOAuthForChildUser}};

// TODO(agawronska): Remove test instantiation with kDMServerOAuthForChildUser
// once it is enabled by default.
INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerTest,
    testing::Combine(
        testing::ValuesIn(feature_lists),
        testing::Values(chromeos::LoggedInUserMixin::LogInType::kRegular)));

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    UserCloudPolicyManagerChildTest,
    testing::Combine(
        testing::ValuesIn(feature_lists),
        testing::Values(chromeos::LoggedInUserMixin::LogInType::kChild)));

}  // namespace policy
