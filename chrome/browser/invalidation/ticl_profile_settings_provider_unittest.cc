// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_profile_settings_provider.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/gcm_channel_status_syncer.h"
#include "components/invalidation/fake_invalidation_state_tracker.h"
#include "components/invalidation/invalidation_state_tracker.h"
#include "components/invalidation/ticl_invalidation_service.h"
#include "components/invalidation/ticl_settings_provider.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class TiclProfileSettingsProviderTest : public testing::Test {
 protected:
  TiclProfileSettingsProviderTest();
  ~TiclProfileSettingsProviderTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  TiclInvalidationService::InvalidationNetworkChannel GetNetworkChannel();

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  FakeOAuth2TokenService token_service_;

  scoped_ptr<TiclInvalidationService> invalidation_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TiclProfileSettingsProviderTest);
};

TiclProfileSettingsProviderTest::TiclProfileSettingsProviderTest() {
}

TiclProfileSettingsProviderTest::~TiclProfileSettingsProviderTest() {
}

void TiclProfileSettingsProviderTest::SetUp() {
  invalidation_service_.reset(new TiclInvalidationService(
      "TestUserAgent",
      scoped_ptr<IdentityProvider>(new FakeIdentityProvider(&token_service_)),
      scoped_ptr<TiclSettingsProvider>(
          new TiclProfileSettingsProvider(&profile_)),
      gcm::GCMProfileServiceFactory::GetForProfile(&profile_)->driver(),
      profile_.GetRequestContext()));
  invalidation_service_->Init(scoped_ptr<syncer::InvalidationStateTracker>(
      new syncer::FakeInvalidationStateTracker));
}

void TiclProfileSettingsProviderTest::TearDown() {
  invalidation_service_.reset();
}

TiclInvalidationService::InvalidationNetworkChannel
TiclProfileSettingsProviderTest::GetNetworkChannel() {
  return invalidation_service_->network_channel_type_;
}

TEST_F(TiclProfileSettingsProviderTest, ChannelSelectionTest) {
  // Default value should be GCM channel.
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());
  PrefService* prefs =  profile_.GetPrefs();

  // If GCM is enabled and invalidation channel setting is not set or set to
  // true then use GCM channel.
  prefs->SetBoolean(gcm::prefs::kGCMChannelStatus, true);
  prefs->SetBoolean(prefs::kInvalidationServiceUseGCMChannel, true);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  prefs->SetBoolean(gcm::prefs::kGCMChannelStatus, true);
  prefs->ClearPref(prefs::kInvalidationServiceUseGCMChannel);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  prefs->ClearPref(gcm::prefs::kGCMChannelStatus);
  prefs->SetBoolean(prefs::kInvalidationServiceUseGCMChannel, true);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  // If invalidation channel setting is set to false, fall back to push channel.
  prefs->SetBoolean(gcm::prefs::kGCMChannelStatus, true);
  prefs->SetBoolean(prefs::kInvalidationServiceUseGCMChannel, false);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  // If invalidation channel setting says use GCM but GCM is not enabled, fall
  // back to push channel.
  prefs->SetBoolean(gcm::prefs::kGCMChannelStatus, false);
  prefs->SetBoolean(prefs::kInvalidationServiceUseGCMChannel, true);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());
}

}  // namespace invalidation
