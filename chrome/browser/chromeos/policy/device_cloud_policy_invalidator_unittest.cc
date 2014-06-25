// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_invalidator.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/invalidation/profile_invalidation_provider.h"
#include "components/invalidation/ticl_invalidation_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/internal_api/public/base/invalidator_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceCloudPolicyInvalidatorTest : public testing::Test {
 public:
  DeviceCloudPolicyInvalidatorTest();
  virtual ~DeviceCloudPolicyInvalidatorTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Ownership is not passed. The Profile is owned by the global ProfileManager.
  Profile *CreateProfile(const std::string& profile_name);

  invalidation::TiclInvalidationService* GetDeviceInvalidationService();
  bool HasDeviceInvalidationServiceObserver() const;

  invalidation::TiclInvalidationService* GetProfileInvalidationService(
      Profile* profile);
  int GetProfileInvalidationServiceObserverCount() const;

  const invalidation::TiclInvalidationService* GetInvalidationService() const;
  bool HasCloudPolicyInvalidator() const;

  void ConnectDeviceInvalidationService();

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_;
  TestingProfileManager profile_manager_;
  scoped_ptr<chromeos::ScopedTestDeviceSettingsService>
      test_device_settings_service_;
  scoped_ptr<chromeos::ScopedTestCrosSettings> test_cros_settings_;

  scoped_ptr<DeviceCloudPolicyInvalidator> invalidator_;
};

DeviceCloudPolicyInvalidatorTest::DeviceCloudPolicyInvalidatorTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      system_request_context_(new net::TestURLRequestContextGetter(
          base::MessageLoopProxy::current())),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
}

DeviceCloudPolicyInvalidatorTest::~DeviceCloudPolicyInvalidatorTest() {
}

void DeviceCloudPolicyInvalidatorTest::SetUp() {
  chromeos::SystemSaltGetter::Initialize();
  chromeos::DBusThreadManager::InitializeWithStub();
  chromeos::DeviceOAuth2TokenServiceFactory::Initialize();
  TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
      system_request_context_.get());
  ASSERT_TRUE(profile_manager_.SetUp());
  test_device_settings_service_.reset(new
      chromeos::ScopedTestDeviceSettingsService);
  test_cros_settings_.reset(new chromeos::ScopedTestCrosSettings);

  invalidator_.reset(new DeviceCloudPolicyInvalidator);
}

void DeviceCloudPolicyInvalidatorTest::TearDown() {
  invalidator_.reset();
  base::RunLoop().RunUntilIdle();

  TestingBrowserProcess::GetGlobal()->SetBrowserPolicyConnector(NULL);
  chromeos::DeviceOAuth2TokenServiceFactory::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  chromeos::SystemSaltGetter::Shutdown();
}

Profile *DeviceCloudPolicyInvalidatorTest::CreateProfile(
    const std::string& profile_name) {
  Profile* profile = profile_manager_.CreateTestingProfile(profile_name);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources(),
      content::Details<Profile>(profile));
  return profile;
}

invalidation::TiclInvalidationService*
DeviceCloudPolicyInvalidatorTest::GetDeviceInvalidationService() {
  return invalidator_->device_invalidation_service_.get();
}

bool DeviceCloudPolicyInvalidatorTest::HasDeviceInvalidationServiceObserver(
    ) const {
  return invalidator_->device_invalidation_service_observer_.get();
}

invalidation::TiclInvalidationService*
DeviceCloudPolicyInvalidatorTest::GetProfileInvalidationService(
    Profile* profile) {
  invalidation::ProfileInvalidationProvider* invalidation_provider =
      static_cast<invalidation::ProfileInvalidationProvider*>(
          invalidation::ProfileInvalidationProviderFactory::GetInstance()->
              GetServiceForBrowserContext(profile, false));
  if (!invalidation_provider)
    return NULL;
  return static_cast<invalidation::TiclInvalidationService*>(
      invalidation_provider->GetInvalidationService());
}

int DeviceCloudPolicyInvalidatorTest::
        GetProfileInvalidationServiceObserverCount() const {
  return invalidator_->profile_invalidation_service_observers_.size();
}

const invalidation::TiclInvalidationService*
DeviceCloudPolicyInvalidatorTest::GetInvalidationService() const {
  return static_cast<invalidation::TiclInvalidationService*>(
      invalidator_->invalidation_service_);
}

bool DeviceCloudPolicyInvalidatorTest::HasCloudPolicyInvalidator() const {
  return invalidator_->invalidator_.get();
}

void DeviceCloudPolicyInvalidatorTest::ConnectDeviceInvalidationService() {
  // Verify that a device-global invalidation service has been created.
  ASSERT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that no per-profile invalidation service observers have been
  // created.
  EXPECT_EQ(0, GetProfileInvalidationServiceObserverCount());

  // Verify that no invalidator exists yet
  EXPECT_FALSE(HasCloudPolicyInvalidator());
  EXPECT_FALSE(GetInvalidationService());

  // Indicate that the device-global invalidation service has connected.
  GetDeviceInvalidationService()->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that an invalidator backed by the device-global invalidation service
  // has been created.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(GetDeviceInvalidationService(), GetInvalidationService());
}

// Verifies that a DeviceCloudPolicyInvalidator backed by a device-global
// invalidation service is created/destroyed as the service
// connects/disconnects.
TEST_F(DeviceCloudPolicyInvalidatorTest, UseDeviceInvalidationService) {
  // Verify that an invalidator backed by the device-global invalidation service
  // is created when the service connects.
  ConnectDeviceInvalidationService();
  ASSERT_TRUE(GetDeviceInvalidationService());

  // Indicate that the device-global invalidation service has disconnected.
  GetDeviceInvalidationService()->OnInvalidatorStateChange(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that the invalidator has been destroyed.
  EXPECT_FALSE(HasCloudPolicyInvalidator());
  EXPECT_FALSE(GetInvalidationService());
}

// Verifies that a DeviceCloudPolicyInvalidator backed by a per-profile
// invalidation service is created/destroyed as the service
// connects/disconnects.
TEST_F(DeviceCloudPolicyInvalidatorTest, UseProfileInvalidationService) {
  // Create a user profile.
  Profile* profile = CreateProfile("test");
  ASSERT_TRUE(profile);

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service has been created.
  invalidation::TiclInvalidationService* profile_invalidation_service =
      GetProfileInvalidationService(profile);
  ASSERT_TRUE(profile_invalidation_service);
  EXPECT_EQ(1, GetProfileInvalidationServiceObserverCount());

  // Verify that no invalidator exists yet
  EXPECT_FALSE(HasCloudPolicyInvalidator());
  EXPECT_FALSE(GetInvalidationService());

  // Indicate that the per-profile invalidation service has connected.
  profile_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidator has been destroyed.
  EXPECT_FALSE(GetDeviceInvalidationService());
  EXPECT_FALSE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service still exists.
  profile_invalidation_service = GetProfileInvalidationService(profile);
  ASSERT_TRUE(profile_invalidation_service);
  EXPECT_EQ(1, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the per-profile invalidation service
  // has been created.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(profile_invalidation_service, GetInvalidationService());

  // Indicate that the per-profile invalidation service has disconnected.
  profile_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  base::RunLoop().RunUntilIdle();

  // Verify that a device-global invalidation service has been created.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service still exists.
  profile_invalidation_service = GetProfileInvalidationService(profile);
  EXPECT_TRUE(profile_invalidation_service);
  EXPECT_EQ(1, GetProfileInvalidationServiceObserverCount());

  // Verify that the invalidator has been destroyed.
  EXPECT_FALSE(HasCloudPolicyInvalidator());
  EXPECT_FALSE(GetInvalidationService());
}

// Verifies that a DeviceCloudPolicyInvalidator exists whenever a connected
// invalidation service is available, automatically switching between
// device-global and per-profile invalidation services as they
// connect/disconnect, giving priority to per-profile invalidation services.
TEST_F(DeviceCloudPolicyInvalidatorTest, SwitchInvalidationServices) {
  // Verify that an invalidator backed by the device-global invalidation service
  // is created when the service connects.
  ConnectDeviceInvalidationService();
  ASSERT_TRUE(GetDeviceInvalidationService());

  // Create a first user profile.
  Profile* profile_1 = CreateProfile("test_1");
  ASSERT_TRUE(profile_1);

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service has been created for the
  // first user profile.
  invalidation::TiclInvalidationService* profile_1_invalidation_service =
      GetProfileInvalidationService(profile_1);
  ASSERT_TRUE(profile_1_invalidation_service);
  EXPECT_EQ(1, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the device-global invalidation service
  // still exists.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(GetDeviceInvalidationService(), GetInvalidationService());

  // Indicate that the first user profile's per-profile invalidation service has
  // connected.
  profile_1_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidator has been destroyed.
  EXPECT_FALSE(GetDeviceInvalidationService());
  EXPECT_FALSE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service still exists for the first
  // user profile.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  EXPECT_TRUE(profile_1_invalidation_service);
  EXPECT_EQ(1, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the per-profile invalidation service
  // for the first user profile has been created.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(profile_1_invalidation_service, GetInvalidationService());

  // Create a second user profile.
  Profile* profile_2 = CreateProfile("test_2");
  ASSERT_TRUE(profile_2);

  // Verify that the device-global invalidator still does not exist.
  EXPECT_FALSE(GetDeviceInvalidationService());
  EXPECT_FALSE(HasDeviceInvalidationServiceObserver());

  // Verify that a per-profile invalidation service still exists for the first
  // user profile and one has been created for the second user profile.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  EXPECT_TRUE(profile_1_invalidation_service);
  invalidation::TiclInvalidationService* profile_2_invalidation_service =
      GetProfileInvalidationService(profile_2);
  ASSERT_TRUE(profile_2_invalidation_service);
  EXPECT_EQ(2, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the per-profile invalidation service
  // for the first user profile still exists.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(profile_1_invalidation_service, GetInvalidationService());

  // Indicate that the second user profile's per-profile invalidation service
  // has connected.
  profile_2_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidator still does not exist.
  EXPECT_FALSE(GetDeviceInvalidationService());
  EXPECT_FALSE(HasDeviceInvalidationServiceObserver());

  // Verify that per-profile invalidation services still exist for both user
  // profiles.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  ASSERT_TRUE(profile_1_invalidation_service);
  profile_2_invalidation_service = GetProfileInvalidationService(profile_2);
  EXPECT_TRUE(profile_2_invalidation_service);
  EXPECT_EQ(2, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the per-profile invalidation service
  // for the first user profile still exists.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(profile_1_invalidation_service, GetInvalidationService());

  // Indicate that the per-profile invalidation service for the first user
  // profile has disconnected.
  profile_1_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidator still does not exist.
  EXPECT_FALSE(GetDeviceInvalidationService());
  EXPECT_FALSE(HasDeviceInvalidationServiceObserver());

  // Verify that per-profile invalidation services still exist for both user
  // profiles.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  EXPECT_TRUE(profile_1_invalidation_service);
  profile_2_invalidation_service = GetProfileInvalidationService(profile_2);
  ASSERT_TRUE(profile_2_invalidation_service);
  EXPECT_EQ(2, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the per-profile invalidation service
  // for the second user profile has been created.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(profile_2_invalidation_service, GetInvalidationService());

  // Indicate that the per-profile invalidation service for the second user
  // profile has disconnected.
  profile_2_invalidation_service->OnInvalidatorStateChange(
      syncer::INVALIDATION_CREDENTIALS_REJECTED);
  base::RunLoop().RunUntilIdle();

  // Verify that a device-global invalidation service has been created.
  ASSERT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that per-profile invalidation services still exist for both user
  // profiles.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  EXPECT_TRUE(profile_1_invalidation_service);
  profile_2_invalidation_service = GetProfileInvalidationService(profile_2);
  EXPECT_TRUE(profile_2_invalidation_service);
  EXPECT_EQ(2, GetProfileInvalidationServiceObserverCount());

  // Verify that the invalidator has been destroyed.
  EXPECT_FALSE(HasCloudPolicyInvalidator());
  EXPECT_FALSE(GetInvalidationService());

  // Indicate that the device-global invalidation service has connected.
  GetDeviceInvalidationService()->OnInvalidatorStateChange(
      syncer::INVALIDATIONS_ENABLED);
  base::RunLoop().RunUntilIdle();

  // Verify that the device-global invalidation service still exists.
  EXPECT_TRUE(GetDeviceInvalidationService());
  EXPECT_TRUE(HasDeviceInvalidationServiceObserver());

  // Verify that per-profile invalidation services still exist for both user
  // profiles.
  profile_1_invalidation_service = GetProfileInvalidationService(profile_1);
  EXPECT_TRUE(profile_1_invalidation_service);
  profile_2_invalidation_service = GetProfileInvalidationService(profile_2);
  EXPECT_TRUE(profile_2_invalidation_service);
  EXPECT_EQ(2, GetProfileInvalidationServiceObserverCount());

  // Verify that an invalidator backed by the device-global invalidation service
  // has been created.
  EXPECT_TRUE(HasCloudPolicyInvalidator());
  EXPECT_EQ(GetDeviceInvalidationService(), GetInvalidationService());
}

}  // namespace policy
