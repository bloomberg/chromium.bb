// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/blimp_location_provider.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "blimp/engine/feature/geolocation/mock_blimp_location_provider_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace blimp {
namespace engine {

class BlimpLocationProviderTest : public testing::Test {
 public:
  BlimpLocationProviderTest()
      : mock_callback_(base::Bind(&BlimpLocationProviderTest::OnLocationUpdate,
                                  base::Unretained(this))),
        delegate_(base::WrapUnique(new MockBlimpLocationProviderDelegate)),
        location_provider_(new BlimpLocationProvider(delegate_->GetWeakPtr())) {
  }

  void SetUp() override {}

  MOCK_METHOD2(OnLocationUpdate,
               void(const device::LocationProvider* provider,
                    const device::Geoposition& geoposition));

 protected:
  device::LocationProvider::LocationProviderUpdateCallback mock_callback_;
  std::unique_ptr<MockBlimpLocationProviderDelegate> delegate_;
  std::unique_ptr<BlimpLocationProvider> location_provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpLocationProviderTest);
};

TEST_F(BlimpLocationProviderTest, StartProviderRunsCorrectly) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::LOW_ACCURACY))
      .Times(1);

  // BlimpLocationProvider implicitly stops on teardown, if it was started.
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  EXPECT_TRUE(location_provider_->StartProvider(true));
  EXPECT_TRUE(location_provider_->StartProvider(false));
}

TEST_F(BlimpLocationProviderTest, StartProviderHandlesNullDelegate) {
  EXPECT_CALL(*delegate_, RequestAccuracy(_)).Times(0);

  delegate_.reset();
  EXPECT_FALSE(location_provider_->StartProvider(true));
  EXPECT_FALSE(location_provider_->StartProvider(false));
}

TEST_F(BlimpLocationProviderTest, StopProviderRunsCorrectly) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_->StopProvider();
}

TEST_F(BlimpLocationProviderTest, StopProviderHandlesNullDelegate) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);

  location_provider_->StartProvider(true);
  delegate_.reset();
  location_provider_->StopProvider();
}

TEST_F(BlimpLocationProviderTest, LocationProviderDeleted) {
  EXPECT_CALL(
      *delegate_,
      RequestAccuracy(GeolocationSetInterestLevelMessage::HIGH_ACCURACY))
      .Times(1);
  EXPECT_CALL(*delegate_,
              RequestAccuracy(GeolocationSetInterestLevelMessage::NO_INTEREST))
      .Times(1);

  location_provider_->StartProvider(true);
  location_provider_.reset();
}

TEST_F(BlimpLocationProviderTest, OnPermissionGranted) {
  EXPECT_CALL(*delegate_, OnPermissionGranted()).Times(1);

  location_provider_->StartProvider(true);
  location_provider_->OnPermissionGranted();
}

TEST_F(BlimpLocationProviderTest, OnPermissionGrantedHandlesNullDelegate) {
  EXPECT_CALL(*delegate_, OnPermissionGranted()).Times(0);

  location_provider_->StartProvider(true);
  delegate_.reset();
  location_provider_->OnPermissionGranted();
}

TEST_F(BlimpLocationProviderTest, SetUpdateCallbackPropagatesCallback) {
  base::Callback<void(const device::Geoposition&)> callback;
  EXPECT_CALL(*delegate_, SetUpdateCallback(_)).WillOnce(SaveArg<0>(&callback));
  EXPECT_CALL(*this, OnLocationUpdate(location_provider_.get(), _)).Times(1);

  location_provider_->SetUpdateCallback(mock_callback_);
  callback.Run(device::Geoposition());
}

TEST_F(BlimpLocationProviderTest, SetUpdateCallbackHandlesNullDelegate) {
  EXPECT_CALL(*delegate_, SetUpdateCallback(_)).Times(0);

  delegate_.reset();
  location_provider_->SetUpdateCallback(mock_callback_);
}

}  // namespace engine
}  // namespace blimp
