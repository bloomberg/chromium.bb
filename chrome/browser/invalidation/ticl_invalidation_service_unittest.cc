// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/invalidation/ticl_invalidation_service.h"

#include "chrome/browser/invalidation/invalidation_service_factory.h"
#include "chrome/browser/invalidation/invalidation_service_test_template.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/test/base/testing_profile.h"
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
    fake_invalidator_ = new syncer::FakeInvalidator();
    profile_.reset(new TestingProfile());
    token_service_.reset(new FakeProfileOAuth2TokenService);
    invalidation_service_.reset(
        new TiclInvalidationService(NULL,
                                    token_service_.get(),
                                    profile_.get()));
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
  scoped_ptr<TiclInvalidationService> invalidation_service_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<FakeProfileOAuth2TokenService> token_service_;
};

INSTANTIATE_TYPED_TEST_CASE_P(
    TiclInvalidationServiceTest, InvalidationServiceTest,
    TiclInvalidationServiceTestDelegate);

}  // namespace invalidation
