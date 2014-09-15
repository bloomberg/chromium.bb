// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_internals_service.h"

#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

namespace {

const char kTestText[] = "abcd1234";

class MockLogReceiver : public password_manager::LogReceiver {
 public:
  MockLogReceiver() {}

  MOCK_METHOD1(LogSavePasswordProgress, void(const std::string&));
};

enum ProfileType { NORMAL_PROFILE, INCOGNITO_PROFILE };

scoped_ptr<TestingProfile> CreateProfile(ProfileType type) {
  TestingProfile::Builder builder;
  scoped_ptr<TestingProfile> profile(builder.Build());
#if !defined(NDEBUG)
  // During the test cases, the profiles may get created on the same address. To
  // avoid over-zealous asserts we need to mark the newly created one as "live".
  // See declaration of MarkBrowserContextLiveForTesting for more details.
  BrowserContextDependencyManager::GetInstance()
      ->MarkBrowserContextLiveForTesting(profile.get());
  if (type == INCOGNITO_PROFILE) {
    BrowserContextDependencyManager::GetInstance()
        ->MarkBrowserContextLiveForTesting(profile->GetOffTheRecordProfile());
  }
#endif
  return profile.Pass();
}

}  // namespace

// When the profile is not incognito, it should be possible to activate the
// service.
TEST(PasswordManagerInternalsServiceTest, ServiceActiveNonIncognito) {
  scoped_ptr<TestingProfile> profile(CreateProfile(NORMAL_PROFILE));
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          profile.get());
  testing::StrictMock<MockLogReceiver> receiver;

  ASSERT_TRUE(profile);
  ASSERT_TRUE(service);
  EXPECT_EQ(std::string(), service->RegisterReceiver(&receiver));

  // TODO(vabr): Use a MockPasswordManagerClient to detect activity changes.
  EXPECT_CALL(receiver, LogSavePasswordProgress(kTestText)).Times(1);
  service->ProcessLog(kTestText);

  service->UnregisterReceiver(&receiver);
}

// When the browser profile is incognito, it should not be possible to activate
// the service.
TEST(PasswordManagerInternalsServiceTest, ServiceNotActiveIncognito) {
  scoped_ptr<TestingProfile> profile(CreateProfile(INCOGNITO_PROFILE));
  ASSERT_TRUE(profile);

  Profile* incognito_profile = profile->GetOffTheRecordProfile();
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(
          incognito_profile);
  // BrowserContextKeyedBaseFactory::GetBrowserContextToUse should return NULL
  // for |profile|, because |profile| is incognito. Therefore the returned
  // |service| should also be NULL.
  EXPECT_FALSE(service);
}
