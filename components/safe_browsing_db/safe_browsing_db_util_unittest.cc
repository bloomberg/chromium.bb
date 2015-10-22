// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "components/safe_browsing_db/safe_browsing_db_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SafeBrowsingDbUtilTest, FullHashOperators) {
  const SBFullHash kHash1 = SBFullHashForString("one");
  const SBFullHash kHash2 = SBFullHashForString("two");

  EXPECT_TRUE(SBFullHashEqual(kHash1, kHash1));
  EXPECT_TRUE(SBFullHashEqual(kHash2, kHash2));
  EXPECT_FALSE(SBFullHashEqual(kHash1, kHash2));
  EXPECT_FALSE(SBFullHashEqual(kHash2, kHash1));

  EXPECT_FALSE(SBFullHashLess(kHash1, kHash2));
  EXPECT_TRUE(SBFullHashLess(kHash2, kHash1));

  EXPECT_FALSE(SBFullHashLess(kHash1, kHash1));
  EXPECT_FALSE(SBFullHashLess(kHash2, kHash2));
}

}  // namespace
