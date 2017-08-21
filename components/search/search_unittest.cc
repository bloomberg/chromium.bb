// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/search.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace search {

#if !defined(OS_IOS) && !defined(OS_ANDROID)

TEST(SearchTest, InstantExtendedAPIEnabled) {
  EXPECT_TRUE(IsInstantExtendedAPIEnabled());
}

TEST(SearchTest, ForceInstantResultsParam) {
  EXPECT_EQ("ion=1&", ForceInstantResultsParam(true));
  EXPECT_EQ(std::string(), ForceInstantResultsParam(false));
}

TEST(SearchTest, InstantExtendedEnabledParam) {
  EXPECT_EQ("espv=2&", InstantExtendedEnabledParam());
}

#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

}  // namespace search
