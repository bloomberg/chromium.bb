// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_manager_base.h"

#include "base/compiler_specific.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/testing_pref_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninManagerBaseTest : public TokenServiceTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    prefs_.reset(new TestingPrefServiceSimple);
    chrome::RegisterLocalState(prefs_->registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(
        prefs_.get());
    TokenServiceTestHarness::SetUp();
    manager_.reset(new SigninManagerBase());
  }

  virtual void TearDown() OVERRIDE {
    // Destroy the SigninManager here, because it relies on profile_ which is
    // freed in the base class.
    manager_->Shutdown();
    manager_.reset(NULL);
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    prefs_.reset(NULL);
    TokenServiceTestHarness::TearDown();
  }

  scoped_ptr<SigninManagerBase> manager_;
  scoped_ptr<TestingPrefServiceSimple> prefs_;
};

TEST_F(SigninManagerBaseTest, Prohibited) {
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerBaseTest, TestAlternateWildcard) {
  // Test to make sure we accept "*@google.com" as a pattern (treat it as if
  // the admin entered ".*@google.com").
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, "*@google.com");
  manager_->Initialize(profile_.get());
  EXPECT_TRUE(manager_->IsAllowedUsername("test@google.com"));
  EXPECT_TRUE(manager_->IsAllowedUsername("happy@google.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@invalid.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername("test@notgoogle.com"));
  EXPECT_FALSE(manager_->IsAllowedUsername(std::string()));
}

TEST_F(SigninManagerBaseTest, ProhibitedAtStartup) {
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  "monkey@invalid.com");
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  manager_->Initialize(profile_.get());
  // Currently signed in user is prohibited by policy, so should be signed out.
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

TEST_F(SigninManagerBaseTest, ProhibitedAfterStartup) {
  std::string user("monkey@invalid.com");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, user);
  manager_->Initialize(profile_.get());
  EXPECT_EQ(user, manager_->GetAuthenticatedUsername());
  // Update the profile - user should be signed out.
  g_browser_process->local_state()->SetString(
      prefs::kGoogleServicesUsernamePattern, ".*@google.com");
  EXPECT_EQ("", manager_->GetAuthenticatedUsername());
}

