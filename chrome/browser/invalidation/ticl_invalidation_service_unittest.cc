// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_invalidation_service.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/invalidation/invalidation_service_test_template.h"
#include "chrome/browser/invalidation/profile_invalidation_auth_provider.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_manager.h"
#include "sync/notifier/fake_invalidation_handler.h"
#include "sync/notifier/fake_invalidator.h"
#include "sync/notifier/invalidation_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class TiclInvalidationServiceTestDelegate {
 public:
  TiclInvalidationServiceTestDelegate() { }

  ~TiclInvalidationServiceTestDelegate() {
    DestroyInvalidationService();
  }

  void CreateInvalidationService() {
    CreateUninitializedInvalidationService();
    InitializeInvalidationService();
  }

  void CreateUninitializedInvalidationService() {
    profile_.reset(new TestingProfile());
    token_service_.reset(new FakeProfileOAuth2TokenService);
    invalidation_service_.reset(new TiclInvalidationService(
        scoped_ptr<InvalidationAuthProvider>(
            new ProfileInvalidationAuthProvider(
                SigninManagerFactory::GetForProfile(profile_.get()),
                token_service_.get(),
                NULL)),
        profile_.get()));
  }

  void InitializeInvalidationService() {
    fake_invalidator_ = new syncer::FakeInvalidator();
    invalidation_service_->InitForTest(fake_invalidator_);
  }

  InvalidationService* GetInvalidationService() {
    return invalidation_service_.get();
  }

  void DestroyInvalidationService() {
    invalidation_service_->Shutdown();
  }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    fake_invalidator_->EmitOnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    fake_invalidator_->EmitOnIncomingInvalidation(invalidation_map);
  }

  syncer::FakeInvalidator* fake_invalidator_;  // owned by the service.
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FakeProfileOAuth2TokenService> token_service_;
  scoped_ptr<TiclInvalidationService> invalidation_service_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    TiclInvalidationServiceTest, InvalidationServiceTest,
    TiclInvalidationServiceTestDelegate);

class TiclInvalidationServiceChannelTest : public ::testing::Test {
 public:
  TiclInvalidationServiceChannelTest() {}
  virtual ~TiclInvalidationServiceChannelTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);
    profile_ = builder.Build();
    fake_signin_manager_ = static_cast<SigninManagerBase*>(
        SigninManagerFactory::GetForProfile(profile_.get()));
    token_service_.reset(new FakeProfileOAuth2TokenService);

    scoped_ptr<InvalidationAuthProvider> auth_provider(
        new ProfileInvalidationAuthProvider(
            fake_signin_manager_, token_service_.get(), NULL));
    invalidation_service_.reset(
        new TiclInvalidationService(auth_provider.Pass(), profile_.get()));
    invalidation_service_->Init();
  }

  virtual void TearDown() OVERRIDE {
    invalidation_service_->Shutdown();
  }

  TiclInvalidationService::InvalidationNetworkChannel GetNetworkChannel() {
    return invalidation_service_->network_channel_type_;
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
  SigninManagerBase* fake_signin_manager_;
  scoped_ptr<FakeProfileOAuth2TokenService> token_service_;
  scoped_ptr<TiclInvalidationService> invalidation_service_;
};

TEST_F(TiclInvalidationServiceChannelTest, ChannelSelectionTest) {
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  // If stars allign use GCM channel.
  profile_->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);
  profile_->GetPrefs()->SetBoolean(prefs::kInvalidationServiceUseGCMChannel,
                                   true);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());

  // If Invalidation channel setting is not set or says false fall back to push
  // channel.
  profile_->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);

  profile_->GetPrefs()->ClearPref(prefs::kInvalidationServiceUseGCMChannel);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  profile_->GetPrefs()->SetBoolean(prefs::kInvalidationServiceUseGCMChannel,
                                   false);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  // If invalidation channel setting says use GCM but GCM is not ALWAYS_ENABLED
  // then fall back to push channel.
  profile_->GetPrefs()->SetBoolean(prefs::kInvalidationServiceUseGCMChannel,
                                   false);

  profile_->GetPrefs()->ClearPref(prefs::kGCMChannelEnabled);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  profile_->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, false);
  EXPECT_EQ(TiclInvalidationService::PUSH_CLIENT_CHANNEL, GetNetworkChannel());

  // If first invalidation setting gets enabled and after that gcm setting gets
  // enabled then should still switch to GCM channel.
  profile_->GetPrefs()->SetBoolean(prefs::kInvalidationServiceUseGCMChannel,
                                   true);
  profile_->GetPrefs()->SetBoolean(prefs::kGCMChannelEnabled, true);
  EXPECT_EQ(TiclInvalidationService::GCM_NETWORK_CHANNEL, GetNetworkChannel());
}

namespace internal {

class FakeCallbackContainer {
  public:
    FakeCallbackContainer()
        : called_(false),
          weak_ptr_factory_(this) { }

    void FakeCallback(const base::DictionaryValue& value) {
      called_ = true;
    }

    bool called_;
    base::WeakPtrFactory<FakeCallbackContainer> weak_ptr_factory_;
};
}  // namespace internal

// Test that requesting for detailed status doesn't crash even if the
// underlying invalidator is not initialized.
TEST(TiclInvalidationServiceLoggingTest, DetailedStatusCallbacksWork) {
  scoped_ptr<TiclInvalidationServiceTestDelegate> delegate (
      new TiclInvalidationServiceTestDelegate());

  delegate->CreateUninitializedInvalidationService();
  invalidation::InvalidationService* const invalidator =
      delegate->GetInvalidationService();

  internal::FakeCallbackContainer fake_container;
  invalidator->RequestDetailedStatus(
    base::Bind(&internal::FakeCallbackContainer::FakeCallback,
               fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_FALSE(fake_container.called_);

  delegate->InitializeInvalidationService();

  invalidator->RequestDetailedStatus(
    base::Bind(&internal::FakeCallbackContainer::FakeCallback,
               fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_container.called_);
}
}  // namespace invalidation
