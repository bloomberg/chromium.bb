// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_invalidator.h"

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/stub_enterprise_install_attributes.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/invalidation/fake_invalidation_service.h"
#include "chrome/browser/policy/cloud/cloud_policy_invalidator.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceCloudPolicyInvalidatorTest : public testing::Test {
 public:
  DeviceCloudPolicyInvalidatorTest();
  ~DeviceCloudPolicyInvalidatorTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  DevicePolicyBuilder device_policy_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<net::URLRequestContextGetter> system_request_context_;
  ScopedStubEnterpriseInstallAttributes install_attributes_;
  scoped_ptr<chromeos::ScopedTestDeviceSettingsService>
      test_device_settings_service_;
  scoped_ptr<chromeos::ScopedTestCrosSettings> test_cros_settings_;
  chromeos::DeviceSettingsTestHelper device_settings_test_helper_;
  TestingProfileManager profile_manager_;
};

DeviceCloudPolicyInvalidatorTest::DeviceCloudPolicyInvalidatorTest()
    : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
      system_request_context_(new net::TestURLRequestContextGetter(
          base::MessageLoopProxy::current())),
      install_attributes_("example.com",
                          "user@example.com",
                          "device_id",
                          DEVICE_MODE_ENTERPRISE),
      profile_manager_(TestingBrowserProcess::GetGlobal()) {
}

DeviceCloudPolicyInvalidatorTest::~DeviceCloudPolicyInvalidatorTest() {
}

void DeviceCloudPolicyInvalidatorTest::SetUp() {
  chromeos::SystemSaltGetter::Initialize();
  chromeos::DBusThreadManager::Initialize();
  chromeos::DeviceOAuth2TokenServiceFactory::Initialize();
  TestingBrowserProcess::GetGlobal()->SetSystemRequestContext(
      system_request_context_.get());
  ASSERT_TRUE(profile_manager_.SetUp());

  test_device_settings_service_.reset(new
      chromeos::ScopedTestDeviceSettingsService);
  test_cros_settings_.reset(new chromeos::ScopedTestCrosSettings);
  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util(
      new ownership::MockOwnerKeyUtil);
  owner_key_util->SetPublicKeyFromPrivateKey(
      *device_policy_.GetSigningKey());
  chromeos::DeviceSettingsService::Get()->SetSessionManager(
      &device_settings_test_helper_,
      owner_key_util);

  device_policy_.policy_data().set_invalidation_source(123);
  device_policy_.policy_data().set_invalidation_name("invalidation");
  device_policy_.Build();
  device_settings_test_helper_.set_policy_blob(device_policy_.GetBlob());
  device_settings_test_helper_.Flush();

  scoped_ptr<MockCloudPolicyClient> policy_client(new MockCloudPolicyClient);
  EXPECT_CALL(*policy_client, SetupRegistration("token", "device-id"));
  CloudPolicyCore* core = TestingBrowserProcess::GetGlobal()->platform_part()->
      browser_policy_connector_chromeos()->GetDeviceCloudPolicyManager()->
          core();
  core->Connect(policy_client.Pass());
  core->StartRefreshScheduler();
}

void DeviceCloudPolicyInvalidatorTest::TearDown() {
  chromeos::DeviceSettingsService::Get()->UnsetSessionManager();
  TestingBrowserProcess::GetGlobal()->SetBrowserPolicyConnector(nullptr);
  chromeos::DeviceOAuth2TokenServiceFactory::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
  chromeos::SystemSaltGetter::Shutdown();
}

// Verifies that an invalidator is created/destroyed as an invalidation service
// becomes available/unavailable. Also verifies that the highest handled
// invalidation version is preserved when switching invalidation services.
TEST_F(DeviceCloudPolicyInvalidatorTest, CreateUseDestroy) {
  CloudPolicyStore* store = static_cast<CloudPolicyStore*>(
      TestingBrowserProcess::GetGlobal()->platform_part()->
          browser_policy_connector_chromeos()->GetDeviceCloudPolicyManager()->
              device_store());
  ASSERT_TRUE(store);

  AffiliatedInvalidationServiceProvider provider;
  DeviceCloudPolicyInvalidator device_policy_invalidator(&provider);

  // Verify that no invalidator exists initially.
  EXPECT_FALSE(device_policy_invalidator.GetInvalidatorForTest());

  // Make a first invalidation service available.
  invalidation::FakeInvalidationService invalidation_service_1;
  device_policy_invalidator.OnInvalidationServiceSet(&invalidation_service_1);

  // Verify that an invalidator backed by the first invalidation service has
  // been created and its highest handled invalidation version starts out as 0.
  CloudPolicyInvalidator* invalidator =
      device_policy_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(0, invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_1,
            invalidator->invalidation_service_for_test());

  // Handle an invalidation with version 1. Verify that the invalidator's
  // highest handled invalidation version is updated accordingly.
  store->Store(device_policy_.policy(), 1);
  invalidator->OnStoreLoaded(store);
  EXPECT_EQ(1, invalidator->highest_handled_invalidation_version());

  // Make the first invalidation service unavailable. Verify that the
  // invalidator is destroyed.
  device_policy_invalidator.OnInvalidationServiceSet(nullptr);
  EXPECT_FALSE(device_policy_invalidator.GetInvalidatorForTest());

  // Make a second invalidation service available.
  invalidation::FakeInvalidationService invalidation_service_2;
  device_policy_invalidator.OnInvalidationServiceSet(&invalidation_service_2);

  // Verify that an invalidator backed by the second invalidation service has
  // been created and its highest handled invalidation version starts out as 1.
  invalidator = device_policy_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(1, invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_2,
            invalidator->invalidation_service_for_test());

  // Make the first invalidation service available again. This implies that the
  // second invalidation service is no longer available.
  device_policy_invalidator.OnInvalidationServiceSet(&invalidation_service_1);

  // Verify that the invalidator backed by the second invalidation service was
  // destroyed and an invalidation backed by the first invalidation service has
  // been created instead. Also verify that its highest handled invalidation
  // version starts out as 1.
  invalidator = device_policy_invalidator.GetInvalidatorForTest();
  ASSERT_TRUE(invalidator);
  EXPECT_EQ(1, invalidator->highest_handled_invalidation_version());
  EXPECT_EQ(&invalidation_service_1,
            invalidator->invalidation_service_for_test());

  provider.Shutdown();
  device_policy_invalidator.OnInvalidationServiceSet(nullptr);
}

}  // namespace policy
