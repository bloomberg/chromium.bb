// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/location_arbitrator.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/geolocation/arbitrator_dependency_factory.h"
#include "chrome/browser/geolocation/fake_access_token_store.h"
#include "chrome/browser/geolocation/geolocation_observer.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/common/geoposition.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockLocationObserver : public GeolocationObserver {
 public:
  void InvalidateLastPosition() {
    last_position_.latitude = 100;
    last_position_.error_code = Geoposition::ERROR_CODE_NONE;
    ASSERT_FALSE(last_position_.IsInitialized());
  }
  // Delegate
  virtual void OnLocationUpdate(const Geoposition& position) {
    last_position_ = position;
  }

  Geoposition last_position_;
};

double g_fake_time_now_secs = 1;

base::Time GetTimeNowForTest() {
  return base::Time::FromDoubleT(g_fake_time_now_secs);
}

void AdvanceTimeNow(const base::TimeDelta& delta) {
  g_fake_time_now_secs += delta.InSecondsF();
}

void SetPositionFix(MockLocationProvider* provider,
                    double latitude,
                    double longitude,
                    double accuracy) {
  Geoposition position;
  position.error_code = Geoposition::ERROR_CODE_NONE;
  position.latitude = latitude;
  position.longitude = longitude;
  position.accuracy = accuracy;
  position.timestamp = GetTimeNowForTest();
  ASSERT_TRUE(position.IsInitialized());
  ASSERT_TRUE(position.IsValidFix());
  provider->HandlePositionChanged(position);
}

void SetReferencePosition(MockLocationProvider* provider) {
  SetPositionFix(provider, 51.0, -0.1, 400);
}

class MockDependencyFactory : public GeolocationArbitratorDependencyFactory {
 public:
  explicit MockDependencyFactory(AccessTokenStore* access_token_store)
      : cell_(NULL),
        gps_(NULL),
        access_token_store_(access_token_store) {
  }
  virtual GeolocationArbitrator::GetTimeNow GetTimeFunction() {
    return GetTimeNowForTest;
  }
  virtual URLRequestContextGetter* GetContextGetter() {
    return NULL;
  }
  virtual AccessTokenStore* NewAccessTokenStore() {
    return access_token_store_.get();
  }
  virtual LocationProviderBase* NewNetworkLocationProvider(
      AccessTokenStore* access_token_store,
      URLRequestContextGetter* context,
      const GURL& url,
      const string16& access_token) {
    return new MockLocationProvider(&cell_);
  }
  virtual LocationProviderBase* NewSystemLocationProvider() {
    return new MockLocationProvider(&gps_);
  }

  // Two location providers, with nice short names to make the tests more
  // readable. Note |gps_| will only be set when there is a high accuracy
  // observer registered (and |cell_| when there's at least one observer of any
  // type).
  MockLocationProvider* cell_;
  MockLocationProvider* gps_;

  scoped_refptr<AccessTokenStore> access_token_store_;
};

class GeolocationLocationArbitratorTest : public testing::Test {
 protected:
  // testing::Test
  virtual void SetUp() {
    access_token_store_ = new FakeAccessTokenStore;
    observer_.reset(new MockLocationObserver);
    dependency_factory_ = new MockDependencyFactory(access_token_store_);
    GeolocationArbitrator::SetDependencyFactoryForTest(
        dependency_factory_.get());
    arbitrator_.reset(GeolocationArbitrator::Create(observer_.get()));
  }

  // testing::Test
  virtual void TearDown() {
    GeolocationArbitrator::SetDependencyFactoryForTest(NULL);
    dependency_factory_ = NULL;
  }

  void CheckLastPositionInfo(double latitude,
                             double longitude,
                             double accuracy) {
    Geoposition geoposition = observer_->last_position_;
    EXPECT_TRUE(geoposition.IsValidFix());
    EXPECT_DOUBLE_EQ(latitude, geoposition.latitude);
    EXPECT_DOUBLE_EQ(longitude, geoposition.longitude);
    EXPECT_DOUBLE_EQ(accuracy, geoposition.accuracy);
  }

  base::TimeDelta SwitchOnFreshnessCliff() {
    // Add 1, to ensure it meets any greater-than test.
    return base::TimeDelta::FromMilliseconds(
        GeolocationArbitrator::kFixStaleTimeoutMilliseconds + 1);
  }

  MockLocationProvider* cell() {
    return dependency_factory_->cell_;
  }

  MockLocationProvider* gps() {
    return dependency_factory_->gps_;
  }

  scoped_refptr<FakeAccessTokenStore> access_token_store_;
  scoped_refptr<MockDependencyFactory> dependency_factory_;
  scoped_ptr<MockLocationObserver> observer_;
  scoped_ptr<GeolocationArbitrator> arbitrator_;
  MessageLoop loop_;
};

TEST_F(GeolocationLocationArbitratorTest, CreateDestroy) {
  EXPECT_TRUE(access_token_store_);
  EXPECT_TRUE(arbitrator_ != NULL);
  arbitrator_.reset();
  SUCCEED();
}

TEST_F(GeolocationLocationArbitratorTest, OnPermissionGranted) {
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGranted());
  arbitrator_->OnPermissionGranted(GURL("http://frame.test"));
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGranted());
  // Can't check the provider has been notified without going through the
  // motions to create the provider (see next test).
  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
}

TEST_F(GeolocationLocationArbitratorTest, NormalUsage) {
  ASSERT_TRUE(access_token_store_);
  ASSERT_TRUE(arbitrator_ != NULL);

  EXPECT_TRUE(access_token_store_->access_token_set_.empty());
  EXPECT_TRUE(access_token_store_->request_);
  EXPECT_TRUE(access_token_store_->access_token_set_.empty());
  ASSERT_TRUE(access_token_store_->request_);

  EXPECT_FALSE(cell());
  EXPECT_FALSE(gps());
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  EXPECT_TRUE(gps());
  EXPECT_TRUE(cell()->has_listeners());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  EXPECT_FALSE(observer_->last_position_.IsInitialized());

  SetReferencePosition(cell());

  EXPECT_TRUE(observer_->last_position_.IsInitialized());
  EXPECT_EQ(cell()->position_.latitude,
            observer_->last_position_.latitude);

  EXPECT_FALSE(
      cell()->permission_granted_url_.is_valid());
  EXPECT_FALSE(arbitrator_->HasPermissionBeenGranted());
  GURL frame_url("http://frame.test");
  arbitrator_->OnPermissionGranted(frame_url);
  EXPECT_TRUE(arbitrator_->HasPermissionBeenGranted());
  EXPECT_TRUE(
      cell()->permission_granted_url_.is_valid());
  EXPECT_EQ(frame_url,
            cell()->permission_granted_url_);
}

TEST_F(GeolocationLocationArbitratorTest, SetObserverOptions) {
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());
  arbitrator_->StartProviders(GeolocationObserverOptions(true));
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, gps()->state_);
  arbitrator_->StartProviders(GeolocationObserverOptions(false));
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
}

TEST_F(GeolocationLocationArbitratorTest, StartProvidersAfterFixArrives) {
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  SetReferencePosition(cell());
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::LOW_ACCURACY, gps()->state_);
  arbitrator_->StartProviders(GeolocationObserverOptions(true));
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, cell()->state_);
  EXPECT_EQ(MockLocationProvider::HIGH_ACCURACY, gps()->state_);
}

TEST_F(GeolocationLocationArbitratorTest, Arbitration) {
  access_token_store_->NotifyDelegateTokensLoaded();
  ASSERT_TRUE(cell());
  ASSERT_TRUE(gps());

  SetPositionFix(cell(), 1, 2, 150);

  // First position available
  EXPECT_TRUE(observer_->last_position_.IsValidFix());
  CheckLastPositionInfo(1, 2, 150);

  SetPositionFix(gps(), 3, 4, 50);

  // More accurate fix available
  CheckLastPositionInfo(3, 4, 50);

  SetPositionFix(cell(), 5, 6, 150);

  // New fix is available but it's less accurate, older fix should be kept.
  CheckLastPositionInfo(3, 4, 50);

  // Advance time, and notify once again
  AdvanceTimeNow(SwitchOnFreshnessCliff());
  cell()->HandlePositionChanged(cell()->position_);

  // New fix is available, less accurate but fresher
  CheckLastPositionInfo(5, 6, 150);

  // Advance time, and set a low accuracy position
  AdvanceTimeNow(SwitchOnFreshnessCliff());
  SetPositionFix(cell(), 5.676731, 139.629385, 1000);
  CheckLastPositionInfo(5.676731, 139.629385, 1000);

  // 15 secs later, step outside. Switches to gps signal.
  AdvanceTimeNow(base::TimeDelta::FromSeconds(15));
  SetPositionFix(gps(), 3.5676457, 139.629198, 50);
  CheckLastPositionInfo(3.5676457, 139.629198, 50);

  // 5 mins later switch cells while walking. Stay on gps.
  AdvanceTimeNow(base::TimeDelta::FromMinutes(5));
  SetPositionFix(cell(), 3.567832, 139.634648, 300);
  SetPositionFix(gps(), 3.5677675, 139.632314, 50);
  CheckLastPositionInfo(3.5677675, 139.632314, 50);

  // Ride train and gps signal degrades slightly. Stay on fresher gps
  AdvanceTimeNow(base::TimeDelta::FromMinutes(5));
  SetPositionFix(gps(), 3.5679026, 139.634777, 300);
  CheckLastPositionInfo(3.5679026, 139.634777, 300);

  // 14 minutes later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(14));

  // GPS reading misses a beat, but don't switch to cell yet to avoid
  // oscillating.
  SetPositionFix(gps(), 3.5659005, 139.682579, 300);

  AdvanceTimeNow(base::TimeDelta::FromSeconds(7));
  SetPositionFix(cell(), 3.5689579, 139.691420, 1000);
  CheckLastPositionInfo(3.5659005, 139.682579, 300);

  // 1 minute later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(1));

  // Enter tunnel. Stay on fresher gps for a moment.
  SetPositionFix(cell(), 3.5657078, 139.68922, 300);
  SetPositionFix(gps(), 3.5657104, 139.690341, 300);
  CheckLastPositionInfo(3.5657104, 139.690341, 300);

  // 2 minutes later
  AdvanceTimeNow(base::TimeDelta::FromMinutes(2));
  // Arrive in station. Cell moves but GPS is stale. Switch to fresher cell.
  SetPositionFix(cell(), 3.5658700, 139.069979, 1000);
  CheckLastPositionInfo(3.5658700, 139.069979, 1000);
}

}  // namespace
