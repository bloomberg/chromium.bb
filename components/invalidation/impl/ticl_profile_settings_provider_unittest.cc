// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/ticl_profile_settings_provider.h"

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/invalidation/impl/fake_invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidation_prefs.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/invalidation/impl/ticl_invalidation_service.h"
#include "components/invalidation/impl/ticl_settings_provider.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "jingle/glue/network_service_config_test_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class TiclProfileSettingsProviderTest : public testing::Test {
 protected:
  TiclProfileSettingsProviderTest();
  ~TiclProfileSettingsProviderTest() override;

  // testing::Test:
  void SetUp() override;

  TiclInvalidationService::InvalidationNetworkChannel GetNetworkChannel();

  base::test::ScopedTaskEnvironment task_environment_;
  std::unique_ptr<jingle_glue::NetworkServiceConfigTestUtil> net_config_helper_;
  gcm::FakeGCMDriver gcm_driver_;

  // |identity_test_env_| should be declared before |identity_provider_|
  // in order to ensure correct destruction order.
  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<invalidation::IdentityProvider> identity_provider_;

  sync_preferences::TestingPrefServiceSyncable pref_service_;

  // The service has to be below the provider since the service keeps
  // a non-owned pointer to the provider.
  std::unique_ptr<TiclInvalidationService> invalidation_service_;

 private:
  void RequestSocket(
      base::WeakPtr<TiclInvalidationService> service,
      network::mojom::ProxyResolvingSocketFactoryRequest request) {
    net_config_helper_->MakeSocketFactoryCallback().Run(std::move(request));
  }
  DISALLOW_COPY_AND_ASSIGN(TiclProfileSettingsProviderTest);
};

TiclProfileSettingsProviderTest::TiclProfileSettingsProviderTest() {}

TiclProfileSettingsProviderTest::~TiclProfileSettingsProviderTest() {}

void TiclProfileSettingsProviderTest::SetUp() {
  gcm::GCMChannelStatusSyncer::RegisterProfilePrefs(pref_service_.registry());
  ProfileInvalidationProvider::RegisterProfilePrefs(pref_service_.registry());

  net_config_helper_ =
      std::make_unique<jingle_glue::NetworkServiceConfigTestUtil>(
          base::MakeRefCounted<net::TestURLRequestContextGetter>(
              base::ThreadTaskRunnerHandle::Get()));
  identity_provider_ = std::make_unique<ProfileIdentityProvider>(
      identity_test_env_.identity_manager());

  invalidation_service_ = std::make_unique<TiclInvalidationService>(
      "TestUserAgent", identity_provider_.get(),
      std::unique_ptr<TiclSettingsProvider>(
          new TiclProfileSettingsProvider(&pref_service_)),
      &gcm_driver_,
      base::BindRepeating(&TiclProfileSettingsProviderTest::RequestSocket,
                          base::Unretained(this)),
      base::ThreadTaskRunnerHandle::Get(), nullptr /* url_loader_factory */,
      network::TestNetworkConnectionTracker::GetInstance());
  invalidation_service_->Init(std::unique_ptr<syncer::InvalidationStateTracker>(
      new syncer::FakeInvalidationStateTracker));
}

TiclInvalidationService::InvalidationNetworkChannel
TiclProfileSettingsProviderTest::GetNetworkChannel() {
  return invalidation_service_->network_channel_type_;
}

TEST_F(TiclProfileSettingsProviderTest, ChannelSelectionTest) {
  // Default value should be GCM channel.
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  // If GCM is enabled then use GCM channel.
  pref_service_.SetBoolean(gcm::prefs::kGCMChannelStatus, true);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  pref_service_.ClearPref(gcm::prefs::kGCMChannelStatus);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  // If invalidation channel setting says use GCM but GCM is not enabled, do not
  // fall back to push channel.
  pref_service_.SetBoolean(gcm::prefs::kGCMChannelStatus, false);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());
}

}  // namespace invalidation
