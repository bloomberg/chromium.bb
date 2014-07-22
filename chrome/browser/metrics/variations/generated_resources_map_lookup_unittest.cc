// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/generated_resources_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_variations {

TEST(GeneratedResourcesMapLookupTest, LookupNotFound) {
  EXPECT_EQ(-1, GetResourceIndex(0));
  EXPECT_EQ(-1, GetResourceIndex(kResourceHashes[kNumResources - 1] + 1));

  // Lookup a hash that shouldn't exist.
  // 13257681U is 1 + the hash for IDS_ZOOM_NORMAL
  EXPECT_EQ(-1, GetResourceIndex(13257681U));
}

TEST(GeneratedResourcesMapLookupTest, LookupFound) {
  for (size_t i = 0; i < kNumResources; ++i)
    EXPECT_EQ(kResourceIndices[i], GetResourceIndex(kResourceHashes[i]));
}

}  // namespace chrome_variations
