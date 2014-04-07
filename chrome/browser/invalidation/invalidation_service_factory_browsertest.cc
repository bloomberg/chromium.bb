// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/invalidation/invalidation_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class InvalidationServiceFactoryTestBase : public InProcessBrowserTest {
 protected:
  InvalidationServiceFactoryTestBase();
  virtual ~InvalidationServiceFactoryTestBase();

  InvalidationService* FindInvalidationServiceForProfile(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactoryTestBase);
};

InvalidationServiceFactoryTestBase::InvalidationServiceFactoryTestBase() {
}

InvalidationServiceFactoryTestBase::~InvalidationServiceFactoryTestBase() {
}

InvalidationService*
InvalidationServiceFactoryTestBase::FindInvalidationServiceForProfile(
    Profile* profile) {
  return static_cast<InvalidationService*>(
      InvalidationServiceFactory::GetInstance()->GetServiceForBrowserContext(
          profile, false));
}

class InvalidationServiceFactoryLoginScreenBrowserTest
    : public InvalidationServiceFactoryTestBase {
 protected:
  InvalidationServiceFactoryLoginScreenBrowserTest();
  virtual ~InvalidationServiceFactoryLoginScreenBrowserTest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactoryLoginScreenBrowserTest);
};

InvalidationServiceFactoryLoginScreenBrowserTest::
    InvalidationServiceFactoryLoginScreenBrowserTest() {
}

InvalidationServiceFactoryLoginScreenBrowserTest::
    ~InvalidationServiceFactoryLoginScreenBrowserTest() {
}

void InvalidationServiceFactoryLoginScreenBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kLoginManager);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
}

// Verify that no InvalidationService is instantiated for the login profile on
// the login screen.
IN_PROC_BROWSER_TEST_F(InvalidationServiceFactoryLoginScreenBrowserTest,
                       NoInvalidationService) {
  Profile* login_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(FindInvalidationServiceForProfile(login_profile));
}

class InvalidationServiceFactoryGuestBrowserTest
    : public InvalidationServiceFactoryTestBase {
 protected:
  InvalidationServiceFactoryGuestBrowserTest();
  virtual ~InvalidationServiceFactoryGuestBrowserTest();

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidationServiceFactoryGuestBrowserTest);
};

InvalidationServiceFactoryGuestBrowserTest::
    InvalidationServiceFactoryGuestBrowserTest() {
}

InvalidationServiceFactoryGuestBrowserTest::
    ~InvalidationServiceFactoryGuestBrowserTest() {
}

void InvalidationServiceFactoryGuestBrowserTest::SetUpCommandLine(
    CommandLine* command_line) {
  command_line->AppendSwitch(chromeos::switches::kGuestSession);
  command_line->AppendSwitch(::switches::kIncognito);
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                  chromeos::UserManager::kGuestUserName);
}

// Verify that no InvalidationService is instantiated for the login profile or
// the guest profile while a guest session is in progress.
IN_PROC_BROWSER_TEST_F(InvalidationServiceFactoryGuestBrowserTest,
                       NoInvalidationService) {
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
  Profile* guest_profile = user_manager->GetProfileByUser(
      user_manager->GetActiveUser())->GetOriginalProfile();
  Profile* login_profile =
      chromeos::ProfileHelper::GetSigninProfile()->GetOriginalProfile();
  EXPECT_FALSE(FindInvalidationServiceForProfile(guest_profile));
  EXPECT_FALSE(FindInvalidationServiceForProfile(login_profile));
}

}  // namespace invalidation
