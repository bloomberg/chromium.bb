// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/service/ui_string_overrider.h"

#include "chrome/browser/metrics/variations/generated_resources_map.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(sdefresne): componentize this tests as part of the resolution
// of http://crbug.com/534257 ("Decouple iOS port from using //chrome's
// VariationsService client").

namespace chrome_variations {

class UIStringOverriderTest : public ::testing::Test {
 public:
  UIStringOverriderTest()
      : provider_(kResourceHashes, kResourceIndices, kNumResources) {}

  int GetResourceIndex(uint32_t hash) {
    return provider_.GetResourceIndex(hash);
  }

 private:
  variations::UIStringOverrider provider_;

  DISALLOW_COPY_AND_ASSIGN(UIStringOverriderTest);
};

TEST_F(UIStringOverriderTest, LookupNotFound) {
  EXPECT_EQ(-1, GetResourceIndex(0));
  EXPECT_EQ(-1, GetResourceIndex(kResourceHashes[kNumResources - 1] + 1));

  // Lookup a hash that shouldn't exist.
  // 13257681U is 1 + the hash for IDS_ZOOM_NORMAL
  EXPECT_EQ(-1, GetResourceIndex(13257681U));
}

TEST_F(UIStringOverriderTest, LookupFound) {
  for (size_t i = 0; i < kNumResources; ++i)
    EXPECT_EQ(kResourceIndices[i], GetResourceIndex(kResourceHashes[i]));
}

}  // namespace chrome_variations
