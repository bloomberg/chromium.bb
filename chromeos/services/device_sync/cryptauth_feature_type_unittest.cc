// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_feature_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToAndFromString) {
  for (CryptAuthFeatureType feature_type : GetAllCryptAuthFeatureTypes()) {
    EXPECT_EQ(feature_type, CryptAuthFeatureTypeFromString(
                                CryptAuthFeatureTypeToString(feature_type)));
  }
}

TEST(DeviceSyncCryptAuthFeatureTypeTest, ToAndFromSoftwareFeature) {
  // SoftwareFeature map onto the "enabled" feature types
  for (CryptAuthFeatureType feature_type : GetEnabledCryptAuthFeatureTypes()) {
    EXPECT_EQ(feature_type,
              CryptAuthFeatureTypeFromSoftwareFeature(
                  CryptAuthFeatureTypeToSoftwareFeature(feature_type)));
  }
}

}  // namespace device_sync

}  // namespace chromeos
