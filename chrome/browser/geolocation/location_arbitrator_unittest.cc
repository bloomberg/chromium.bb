// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/location_arbitrator.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/geolocation/fake_access_token_store.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/common/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
  position->timestamp = base::Time::FromDoubleT(87654321.0);
  ASSERT_TRUE(position->IsInitialized());
  ASSERT_TRUE(position->IsValidFix());
}

}  // namespace

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    access_token_store_ = new FakeAccessTokenStore;
    GeolocationArbitrator::SetProviderFactoryForTest(&NewMockLocationProvider);
    arbitrator_ = GeolocationArbitrator::Create(access_token_store_.get(),
                                                NULL);
  }

  virtual void TearDown() {
  }

  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  scoped_refptr<GeolocationArbitrator> arbitrator_;
};

TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  EXPECT_TRUE(access_token_store_);
  EXPECT_TRUE(arbitrator_ != NULL);
  arbitrator_ = NULL;
  SUCCEED();
}

TEST_F(GeolocationLocationArbitratorTest, NormalUsage) {
  ASSERT_TRUE(access_token_store_);
  ASSERT_TRUE(arbitrator_ != NULL);

  EXPECT_TRUE(access_token_store_->access_token_set_.empty());
  EXPECT_FALSE(access_token_store_->request_);
  MockLocationObserver observer;
  arbitrator_->AddObserver(&observer, GeolocationArbitrator::UpdateOptions());

  EXPECT_TRUE(access_token_store_->access_token_set_.empty());
  ASSERT_TRUE(access_token_store_->request_);

  EXPECT_FALSE(MockLocationProvider::instance_);
  access_token_store_->NotifyDelegateTokensLoaded();
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

  access_token_store_->NotifyDelegateTokensLoaded();
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

TEST_F(GeolocationLocationArbitratorTest,
       MultipleAddObserverCallsFromSameListener) {
  MockLocationObserver observer;
  arbitrator_->AddObserver(
      &observer, GeolocationArbitrator::UpdateOptions(false));
  // TODO(joth): Check this causes the GPS provider to fire up.
  arbitrator_->AddObserver(
      &observer, GeolocationArbitrator::UpdateOptions(true));
  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer));
  EXPECT_FALSE(arbitrator_->RemoveObserver(&observer));
}

TEST_F(GeolocationLocationArbitratorTest, RegistrationAfterFixArrives) {
  MockLocationObserver observer1;
  arbitrator_->AddObserver(&observer1, GeolocationArbitrator::UpdateOptions());

  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(MockLocationProvider::instance_);
  EXPECT_FALSE(observer1.last_position_.IsInitialized());
  SetReferencePosition(&MockLocationProvider::instance_->position_);
  MockLocationProvider::instance_->UpdateListeners();
  EXPECT_TRUE(observer1.last_position_.IsValidFix());

  MockLocationObserver observer2;
  EXPECT_FALSE(observer2.last_position_.IsValidFix());
  arbitrator_->AddObserver(&observer2, GeolocationArbitrator::UpdateOptions());
  EXPECT_TRUE(observer2.last_position_.IsValidFix());

  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer1));
  EXPECT_TRUE(arbitrator_->RemoveObserver(&observer2));
}
