// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/location_arbitrator.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/geolocation/fake_access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/common/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Mock implementation of a location provider for testing.
class MockLocationProvider : public LocationProviderBase {
 public:
  MockLocationProvider() : started_count_(0) {
    CHECK(instance_ == NULL);
    instance_ = this;
  }
  virtual ~MockLocationProvider() {
    CHECK(instance_ == this);
    instance_ = NULL;
  }

  using LocationProviderBase::UpdateListeners;
  using LocationProviderBase::InformListenersOfMovement;

  // LocationProviderBase implementation.
  virtual bool StartProvider() {
    ++started_count_;
    return true;
  }
  virtual void GetPosition(Geoposition *position) {
    *position = position_;
  }

  Geoposition position_;
  int started_count_;

  static MockLocationProvider* instance_;

  DISALLOW_COPY_AND_ASSIGN(MockLocationProvider);
};

MockLocationProvider* MockLocationProvider::instance_ = NULL;

class MockLocationObserver : public GeolocationArbitrator::Delegate {
 public:
  void InvalidateLastPosition() {
    last_position_.accuracy = -1;
    last_position_.error_code = Geoposition::ERROR_CODE_NONE;
    ASSERT_FALSE(last_position_.IsInitialized());
  }
  // Delegate
  virtual void OnLocationUpdate(const Geoposition& position) {
    last_position_ = position;
  }

  Geoposition last_position_;
};

void SetReferencePosition(Geoposition* position) {
  position->latitude = 51.0;
  position->longitude = -0.1;
  position->accuracy = 400;
  position->timestamp = 87654321;
  ASSERT_TRUE(position->IsInitialized());
  ASSERT_TRUE(position->IsValidFix());
}

}  // namespace

LocationProviderBase* NewMockLocationProvider() {
  return new MockLocationProvider;
}

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    access_token_factory_ = new FakeAccessTokenStoreFactory;
    arbitrator_.reset(GeolocationArbitrator::New(access_token_factory_.get(),
                                                 NULL));
    arbitrator_->SetUseMockProvider(true);
  }

  virtual void TearDown() {
  }

  scoped_refptr<FakeAccessTokenStoreFactory> access_token_factory_;
  scoped_ptr<GeolocationArbitrator> arbitrator_;
};

TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  EXPECT_TRUE(access_token_factory_);
  EXPECT_TRUE(arbitrator_ != NULL);
  arbitrator_.reset();
  SUCCEED();
}

TEST_F(GeolocationLocationArbitratorTest, NormalUsage) {
  ASSERT_TRUE(access_token_factory_);
  ASSERT_TRUE(arbitrator_ != NULL);

  EXPECT_FALSE(access_token_factory_->default_url_.is_valid());
  EXPECT_FALSE(access_token_factory_->delegate_);
  MockLocationObserver observer;
  arbitrator_->AddObserver(&observer, GeolocationArbitrator::UpdateOptions());

  EXPECT_TRUE(access_token_factory_->default_url_.is_valid());
  ASSERT_TRUE(access_token_factory_->delegate_);

  EXPECT_FALSE(MockLocationProvider::instance_);
  access_token_factory_->NotifyDelegateStoreCreated();
  ASSERT_TRUE(MockLocationProvider::instance_);
  EXPECT_TRUE(MockLocationProvider::instance_->has_listeners());
  EXPECT_EQ(1, MockLocationProvider::instance_->started_count_);
  EXPECT_FALSE(observer.last_position_.IsInitialized());

  SetReferencePosition(&MockLocationProvider::instance_->position_);
  MockLocationProvider::instance_->UpdateListeners();
  EXPECT_TRUE(observer.last_position_.IsInitialized());
  EXPECT_EQ(MockLocationProvider::instance_->position_.latitude,
            observer.last_position_.latitude);

  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer));
}

TEST_F(GeolocationLocationArbitratorTest, MultipleListener) {
  MockLocationObserver observer1;
  arbitrator_->AddObserver(&observer1, GeolocationArbitrator::UpdateOptions());
  MockLocationObserver observer2;
  arbitrator_->AddObserver(&observer2, GeolocationArbitrator::UpdateOptions());

  access_token_factory_->NotifyDelegateStoreCreated();
  ASSERT_TRUE(MockLocationProvider::instance_);
  EXPECT_FALSE(observer1.last_position_.IsInitialized());
  EXPECT_FALSE(observer2.last_position_.IsInitialized());

  SetReferencePosition(&MockLocationProvider::instance_->position_);
  MockLocationProvider::instance_->UpdateListeners();
  EXPECT_TRUE(observer1.last_position_.IsInitialized());
  EXPECT_TRUE(observer2.last_position_.IsInitialized());

  // Add third observer, and remove the first.
  MockLocationObserver observer3;
  arbitrator_->AddObserver(&observer3, GeolocationArbitrator::UpdateOptions());
  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer1));
  observer1.InvalidateLastPosition();
  observer2.InvalidateLastPosition();
  observer3.InvalidateLastPosition();

  MockLocationProvider::instance_->UpdateListeners();
  EXPECT_FALSE(observer1.last_position_.IsInitialized());
  EXPECT_TRUE(observer2.last_position_.IsInitialized());
  EXPECT_TRUE(observer3.last_position_.IsInitialized());

  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer2));
  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer3));
}

TEST_F(GeolocationLocationArbitratorTest, MultipleRegistrations) {
  MockLocationObserver observer;
  arbitrator_->AddObserver(&observer, GeolocationArbitrator::UpdateOptions());
  GeolocationArbitrator::UpdateOptions high_accuracy_options;
  high_accuracy_options.use_high_accuracy = true;
  // TODO(joth): Check this causes the GPS provider to fire up.
  arbitrator_->AddObserver(&observer, high_accuracy_options);
  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer));
  EXPECT_FALSE(arbitrator_->RemoveObserver(&observer));
}
