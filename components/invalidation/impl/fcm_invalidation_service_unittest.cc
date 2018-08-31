// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/fcm_invalidation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/invalidation/impl/fcm_fake_invalidator.h"
#include "components/invalidation/impl/gcm_invalidation_bridge.h"
#include "components/invalidation/impl/invalidation_service_test_template.h"
#include "components/invalidation/impl/invalidation_state_tracker.h"
#include "components/invalidation/impl/invalidator.h"
#include "components/invalidation/impl/json_unsafe_parser.h"
#include "components/invalidation/impl/profile_identity_provider.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/identity/public/cpp/identity_test_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {

class FCMInvalidationServiceTestDelegate {
 public:
  FCMInvalidationServiceTestDelegate() {}

  ~FCMInvalidationServiceTestDelegate() {}

  void CreateInvalidationService() {
    CreateUninitializedInvalidationService();
    InitializeInvalidationService();
  }

  void CreateUninitializedInvalidationService() {
    gcm_driver_.reset(new gcm::FakeGCMDriver());
    invalidation_service_.reset(new FCMInvalidationService(
        std::make_unique<ProfileIdentityProvider>(
            identity_test_env_.identity_manager()),
        gcm_driver_.get(), nullptr, nullptr,
        base::BindRepeating(&syncer::JsonUnsafeParser::Parse),
        &url_loader_factory_));
  }

  void InitializeInvalidationService() {
    fake_invalidator_ = new syncer::FCMFakeInvalidator();
    invalidation_service_->InitForTest(fake_invalidator_);
  }

  InvalidationService* GetInvalidationService() {
    return invalidation_service_.get();
  }

  void DestroyInvalidationService() { invalidation_service_.reset(); }

  void TriggerOnInvalidatorStateChange(syncer::InvalidatorState state) {
    fake_invalidator_->EmitOnInvalidatorStateChange(state);
  }

  void TriggerOnIncomingInvalidation(
      const syncer::ObjectIdInvalidationMap& invalidation_map) {
    fake_invalidator_->EmitOnIncomingInvalidation(invalidation_map);
  }

  identity::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<gcm::GCMDriver> gcm_driver_;
  syncer::FCMFakeInvalidator* fake_invalidator_;  // Owned by the service.
  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<FCMInvalidationService> invalidation_service_;
};

INSTANTIATE_TYPED_TEST_CASE_P(FCMInvalidationServiceTest,
                              InvalidationServiceTest,
                              FCMInvalidationServiceTestDelegate);

namespace internal {

class FakeCallbackContainer {
 public:
  FakeCallbackContainer() : called_(false), weak_ptr_factory_(this) {}

  void FakeCallback(const base::DictionaryValue& value) { called_ = true; }

  bool called_;
  base::WeakPtrFactory<FakeCallbackContainer> weak_ptr_factory_;
};

}  // namespace internal

// Test that requesting for detailed status doesn't crash even if the
// underlying invalidator is not initialized.
TEST(FCMInvalidationServiceLoggingTest, DetailedStatusCallbacksWork) {
  std::unique_ptr<FCMInvalidationServiceTestDelegate> delegate(
      new FCMInvalidationServiceTestDelegate());

  delegate->CreateUninitializedInvalidationService();
  invalidation::InvalidationService* const invalidator =
      delegate->GetInvalidationService();

  internal::FakeCallbackContainer fake_container;
  invalidator->RequestDetailedStatus(
      base::BindRepeating(&internal::FakeCallbackContainer::FakeCallback,
                          fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_FALSE(fake_container.called_);

  delegate->InitializeInvalidationService();

  invalidator->RequestDetailedStatus(
      base::BindRepeating(&internal::FakeCallbackContainer::FakeCallback,
                          fake_container.weak_ptr_factory_.GetWeakPtr()));
  EXPECT_TRUE(fake_container.called_);
}

}  // namespace invalidation
