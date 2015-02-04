// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/affiliated_invalidation_service_provider.h"

#include <string>

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/invalidation/invalidation_service.h"
#include "components/invalidation/invalidator_state.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/ticl_invalidation_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::StrictMock;

namespace policy {

namespace {

const char kAffiliatedUserID1[] = "test_1@example.com";
const char kAffiliatedUserID2[] = "test_2@example.com";
const char kUnaffiliatedUserID[] = "test@other_domain.test";

KeyedService* BuildProfileInvalidationProvider(
    content::BrowserContext* context) {
  scoped_ptr<invalidation::FakeInvalidationService> invalidation_service(
      new invalidation::FakeInvalidationService);
  invalidation_service->SetInvalidatorState(
      syncer::TRANSIENT_INVALIDATION_ERROR);
  return new invalidation::ProfileInvalidationProvider(
      invalidation_service.Pass());
}

}  // namespace

class MockConsumer : public AffiliatedInvalidationServiceProvider::Consumer {
 public:
  MockConsumer();
  ~MockConsumer() override;

  MOCK_METHOD1(OnInvalidationServiceSet,
               void(invalidation::InvalidationService*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConsumer);
};

class AffiliatedInvalidationServiceProviderTest : public testing::Test {
 public:
  AffiliatedInvalidationServiceProviderTest();

  // testing::Test:
  virtual void SetUp() override;
  virtual void TearDown() override;

  // Ownership is not passed. The Profile is owned by the global ProfileManager.
  Profile* LogInAndReturnProfile(const std::string& user_id);

  // Logs in as an affiliated user and indicates that the per-profile
  // invalidation service for this user connected. Verifies that this
  // invalidation service is made available to the |consumer_| and the
  // device-global invalidation service is destroyed.
  void LogInAsAffiliatedUserAndConnectInvalidationService();

  // Logs in as an unaffiliated user and indicates that the per-profile
  // invalidation service for this user connected. Verifies that this
  // invalidation service is ignored and the device-global invalidation service
  // is not destroyed.
  void LogInAsUnaffiliatedUserAndConnectInvalidationService();

  // Indicates that the device-global invalidation service connected. Verifies
  // that the |consumer_| is informed about this.
  void ConnectDeviceGlobalInvalidationService();

  // Indicates that the logged-in user's per-profile invalidation service
  // disconnected. Verifies that the |consumer_| is informed about this and a
  // device-global invalidation service is created.
  void DisconnectPerProfileInvalidationService();

  invalidation::FakeInvalidationService* GetProfileInvalidationService(
      Profile* profile,
      bool create);

 protected:
  scoped_ptr<AffiliatedInvalidationServiceProvider> provider_;
  StrictMock<MockConsumer> consumer_;
  invalidation::TiclInvalidationService* device_invalidation_service_;
  invalidation::FakeInvalidationService* profile_invalidation_service_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  chromeos::FakeChromeUserManager* fake_user_manager_;
  chromeos::ScopedUserManagerEnabler user_manager_enabler_;
  ScopedStubEnterpriseInstallAttributes install_attributes_;
  scoped_ptr<chromeos::ScopedTestDeviceSettingsService>
      test_device_settings_service_;
  scoped_ptr<chromeos::ScopedTestCrosSettings> test_cros_settings_;
  TestingProfileManager profile_manager_;
};

MockConsumer::MockConsumer() {
}

MockConsumer::~MockConsumer() {
}

AffiliatedInvalidationServiceProviderTest::
AffiliatedInvalidationServiceProviderTest()
    : device_invalidation_service_(nullptr),
      profile_invalidation_service_(nullptr),
      fake_user_manager_(new chromeos::FakeChromeUserManager),
      user_manager_enabler_(fake_user_manager_),
      install_attributes_("example.com",
                          "user@example.com",
                          "device_id",
                          DEVICE_MODE_ENTERPRISE),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
}

void AffiliatedInvalidationServiceProviderTest::SetUp() {
  chromeos::SystemSaltGetter::Initialize();
  chromeos::DBusThreadManager::Initialize();
  ASSERT_TRUE(profile_manager_.SetUp());

  test_device_settings_service_.reset(new
      chromeos::ScopedTestDeviceSettingsService);
  test_cros_settings_.reset(new chromeos::ScopedTestCrosSettings);
  chromeos::DeviceOAuth2TokenServiceFactory::Initialize();

  invalidation::ProfileInvalidationProviderFactory::GetInstance()->
      RegisterTestingFactory(BuildProfileInvalidationProvider);

  provider_.reset(new AffiliatedInvalidationServiceProvider);
}

void AffiliatedInvalidationServiceProviderTest::TearDown() {
  provider_->Shutdown();
  provider_.reset();

  invalidation::ProfileInvalidationProviderFactory::GetInstance()->
      RegisterTestingFactory(nullptr);
  chromeos::DeviceOAuth2TokenServiceFactory::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  chromeos::SystemSaltGetter::Shutdown();
}

Profile* AffiliatedInvalidationServiceProviderTest::LogInAndReturnProfile(
    const std::string& user_id) {
  fake_user_manager_->AddUser(user_id);
  Profile* profile = profile_manager_.CreateTestingProfile(user_id);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));
  return profile;
}

void AffiliatedInvalidationServiceProviderTest::
    LogInAsAffiliatedUserAndConnectInvalidationService() {
  Mock::VerifyAndClearExpectations(&consumer_);

  // Log in as an affiliated user.
  Profile* profile = LogInAndReturnProfile(kAffiliatedUserID1);
  EXPECT_TRUE(profile);
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that a per-profile invalidation service has been created.
  profile_invalidation_service_ =
      GetProfileInvalidationService(profile, false /* create */);
  ASSERT_TRUE(profile_invalidation_service_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Indicate that the per-profile invalidation service has connected. Verify
  // that the consumer is informed about this.
  EXPECT_CALL(consumer_,
              OnInvalidationServiceSet(profile_invalidation_service_)).Times(1);
  profile_invalidation_service_->SetInvalidatorState(
      syncer::INVALIDATIONS_ENABLED);
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that the device-global invalidation service has been destroyed.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  Mock::VerifyAndClearExpectations(&consumer_);
}

void AffiliatedInvalidationServiceProviderTest::
    LogInAsUnaffiliatedUserAndConnectInvalidationService() {
  Mock::VerifyAndClearExpectations(&consumer_);

  // Log in as an unaffiliated user.
  Profile* profile = LogInAndReturnProfile(kUnaffiliatedUserID);
  EXPECT_TRUE(profile);

  // Verify that a per-profile invalidation service has been created.
  profile_invalidation_service_ =
      GetProfileInvalidationService(profile, false /* create */);
  ASSERT_TRUE(profile_invalidation_service_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Indicate that the per-profile invalidation service has connected. Verify
  // that the consumer is not called back.
  profile_invalidation_service_->SetInvalidatorState(
      syncer::INVALIDATIONS_ENABLED);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  Mock::VerifyAndClearExpectations(&consumer_);
}

void AffiliatedInvalidationServiceProviderTest::
    ConnectDeviceGlobalInvalidationService() {
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that a device-global invalidation service has been created.
  device_invalidation_service_ =
      provider_->GetDeviceInvalidationServiceForTest();
  ASSERT_TRUE(device_invalidation_service_);

  // Indicate that the device-global invalidation service has connected. Verify
  // that the consumer is informed about this.
  EXPECT_CALL(consumer_, OnInvalidationServiceSet(device_invalidation_service_))
      .Times(1);
  device_invalidation_service_->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);

  Mock::VerifyAndClearExpectations(&consumer_);
}

void AffiliatedInvalidationServiceProviderTest::
    DisconnectPerProfileInvalidationService() {
  Mock::VerifyAndClearExpectations(&consumer_);

  ASSERT_TRUE(profile_invalidation_service_);

  // Indicate that the per-profile invalidation service has disconnected. Verify
  // that the consumer is informed about this.
  EXPECT_CALL(consumer_, OnInvalidationServiceSet(nullptr)).Times(1);
  profile_invalidation_service_->SetInvalidatorState(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  Mock::VerifyAndClearExpectations(&consumer_);
}

invalidation::FakeInvalidationService*
AffiliatedInvalidationServiceProviderTest::GetProfileInvalidationService(
    Profile* profile, bool create) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      static_cast<invalidation::ProfileInvalidationProvider*>(
          invalidation::ProfileInvalidationProviderFactory::GetInstance()->
              GetServiceForBrowserContext(profile, create));
  if (!invalidation_provider)
    return nullptr;
  return static_cast<invalidation::FakeInvalidationService*>(
      invalidation_provider->GetInvalidationService());
}

// No consumers are registered with the AffiliatedInvalidationServiceProvider.
// Verifies that no device-global invalidation service is created, whether an
// affiliated user is logged in or not.
TEST_F(AffiliatedInvalidationServiceProviderTest, NoConsumers) {
  // Verify that no device-global invalidation service has been created.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user.
  EXPECT_TRUE(LogInAndReturnProfile(kAffiliatedUserID1));

  // Verify that no device-global invalidation service has been created.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider.
// Verifies that when no per-profile invalidation service belonging to an
// affiliated user is available, a device-global invalidation service is
// created. Further verifies that when the device-global invalidation service
// connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       UseDeviceInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Indicate that the device-global invalidation service has disconnected.
  // Verify that the consumer is informed about this.
  EXPECT_CALL(consumer_, OnInvalidationServiceSet(nullptr)).Times(1);
  device_invalidation_service_->OnInvalidatorStateChange(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider.
// Verifies that when a per-profile invalidation service belonging to an
// affiliated user connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       UseAffiliatedProfileInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Indicate that the logged-in user's per-profile invalidation service
  // disconnected. Verify that the consumer is informed about this and a
  // device-global invalidation service is created.
  DisconnectPerProfileInvalidationService();

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider.
// Verifies that when a per-profile invalidation service belonging to an
// unaffiliated user connects, it is ignored.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       DoNotUseUnaffiliatedProfileInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an unaffiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is ignored and the device-global invalidation service is not
  // destroyed.
  LogInAsUnaffiliatedUserAndConnectInvalidationService();

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// device-global invalidation service exists, is connected and is made available
// to the consumer. Verifies that when a per-profile invalidation service
// belonging to an affiliated user connects, it is made available to the
// consumer instead and the device-global invalidation service is destroyed.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       SwitchToAffiliatedProfileInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// device-global invalidation service exists, is connected and is made available
// to the consumer. Verifies that when a per-profile invalidation service
// belonging to an unaffiliated user connects, it is ignored and the
// device-global invalidation service continues to be made available to the
// consumer.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       DoNotSwitchToUnaffiliatedProfileInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Log in as an unaffiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is ignored and the device-global invalidation service is not
  // destroyed.
  LogInAsUnaffiliatedUserAndConnectInvalidationService();

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// per-profile invalidation service belonging to an affiliated user exists, is
// connected and is made available to the consumer. Verifies that when the
// per-profile invalidation service disconnects, a device-global invalidation
// service is created. Further verifies that when the device-global invalidation
// service connects, it is made available to the consumer.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       SwitchToDeviceInvalidationService) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as an affiliated user and indicate that the per-profile invalidation
  // service for this user connected. Verify that this invalidation service is
  // made available to the |consumer_| and the device-global invalidation
  // service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Indicate that the logged-in user's per-profile invalidation service
  // disconnected. Verify that the consumer is informed about this and a
  // device-global invalidation service is created.
  DisconnectPerProfileInvalidationService();

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// per-profile invalidation service belonging to a first affiliated user exists,
// is connected and is made available to the consumer. A per-profile
// invalidation service belonging to a second affiliated user also exists and is
// connected. Verifies that when the per-profile invalidation service belonging
// to the first user disconnects, the per-profile invalidation service belonging
// to the second user is made available to the consumer instead.
TEST_F(AffiliatedInvalidationServiceProviderTest,
       SwitchBetweenAffiliatedProfileInvalidationServices) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a first affiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is made available to the |consumer_| and the device-global
  // invalidation service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Log in as a second affiliated user.
  Profile* second_profile = LogInAndReturnProfile(kAffiliatedUserID2);
  EXPECT_TRUE(second_profile);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Verify that a per-profile invalidation service for the second user has been
  // created.
  invalidation::FakeInvalidationService* second_profile_invalidation_service =
      GetProfileInvalidationService(second_profile, false /* create */);
  ASSERT_TRUE(second_profile_invalidation_service);

  // Indicate that the second user's per-profile invalidation service has
  // connected. Verify that the consumer is not called back.
  second_profile_invalidation_service->SetInvalidatorState(
      syncer::INVALIDATIONS_ENABLED);
  Mock::VerifyAndClearExpectations(&consumer_);

  // Indicate that the first user's per-profile invalidation service has
  // disconnected. Verify that the consumer is informed that the second user's
  // per-profile invalidation service should be used instead of the first
  // user's.
  EXPECT_CALL(consumer_,
              OnInvalidationServiceSet(second_profile_invalidation_service))
      .Times(1);
  profile_invalidation_service_->SetInvalidatorState(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// device-global invalidation service exists, is connected and is made available
// to the consumer. Verifies that when a second consumer registers, the
// device-global invalidation service is made available to it as well. Further
// verifies that when the first consumer unregisters, the device-global
// invalidation service is not destroyed and remains available to the second
// consumer. Further verifies that when the second consumer also unregisters,
// the device-global invalidation service is destroyed.
TEST_F(AffiliatedInvalidationServiceProviderTest, MultipleConsumers) {
  // Register a first consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Indicate that the device-global invalidation service connected. Verify that
  // that the consumer is informed about this.
  ConnectDeviceGlobalInvalidationService();

  // Register a second consumer. Verify that the consumer is called back
  // immediately as a connected invalidation service is available.
  StrictMock<MockConsumer> second_consumer;
  EXPECT_CALL(second_consumer,
              OnInvalidationServiceSet(device_invalidation_service_)).Times(1);
  provider_->RegisterConsumer(&second_consumer);
  Mock::VerifyAndClearExpectations(&second_consumer);

  // Unregister the first consumer.
  provider_->UnregisterConsumer(&consumer_);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Unregister the second consumer.
  provider_->UnregisterConsumer(&second_consumer);

  // Verify that the device-global invalidation service has been destroyed.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());
  Mock::VerifyAndClearExpectations(&consumer_);
  Mock::VerifyAndClearExpectations(&second_consumer);
}

// A consumer is registered with the AffiliatedInvalidationServiceProvider. A
// per-profile invalidation service belonging to a first affiliated user exists,
// is connected and is made available to the consumer. Verifies that when the
// provider is shut down, the consumer is informed that no invalidation service
// is available for use anymore. Also verifies that no device-global
// invalidation service is created and a per-profile invalidation service
// belonging to a second affiliated user that subsequently connects is ignored.
TEST_F(AffiliatedInvalidationServiceProviderTest, NoServiceAfterShutdown) {
  // Register a consumer. Verify that the consumer is not called back
  // immediately as no connected invalidation service exists yet.
  provider_->RegisterConsumer(&consumer_);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a first affiliated user and indicate that the per-profile
  // invalidation service for this user connected. Verify that this invalidation
  // service is made available to the |consumer_| and the device-global
  // invalidation service is destroyed.
  LogInAsAffiliatedUserAndConnectInvalidationService();

  // Shut down the |provider_|. Verify that the |consumer_| is informed that no
  // invalidation service is available for use anymore.
  EXPECT_CALL(consumer_, OnInvalidationServiceSet(nullptr)).Times(1);
  provider_->Shutdown();
  Mock::VerifyAndClearExpectations(&consumer_);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Log in as a second affiliated user.
  Profile* second_profile = LogInAndReturnProfile(kAffiliatedUserID2);
  EXPECT_TRUE(second_profile);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Create a per-profile invalidation service for the second user.
  invalidation::FakeInvalidationService* second_profile_invalidation_service =
      GetProfileInvalidationService(second_profile, true /* create */);
  ASSERT_TRUE(second_profile_invalidation_service);

  // Indicate that the second user's per-profile invalidation service has
  // connected. Verify that the consumer is not called back.
  second_profile_invalidation_service->SetInvalidatorState(
      syncer::INVALIDATIONS_ENABLED);

  // Verify that the device-global invalidation service still does not exist.
  EXPECT_FALSE(provider_->GetDeviceInvalidationServiceForTest());

  // Unregister the consumer.
  provider_->UnregisterConsumer(&consumer_);
  Mock::VerifyAndClearExpectations(&consumer_);
}

}  // namespace policy
