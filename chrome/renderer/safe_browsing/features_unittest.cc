// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/features.h"

#include "base/format_macros.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(PhishingFeaturesTest, TooManyFeatures) {
  FeatureMap features;
  for (size_t i = 0; i < FeatureMap::kMaxFeatureMapSize; ++i) {
    EXPECT_TRUE(features.AddBooleanFeature(StringPrintf("Feature%" PRIuS, i)));
  }
  EXPECT_EQ(FeatureMap::kMaxFeatureMapSize, features.features().size());

  // Attempting to add more features should fail.
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_FALSE(features.AddBooleanFeature(StringPrintf("Extra%" PRIuS, i)));
  }
  EXPECT_EQ(FeatureMap::kMaxFeatureMapSize, features.features().size());
}

}  // namespace safe_browsing
