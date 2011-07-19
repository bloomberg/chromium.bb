// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/common/random.h"

TEST(SyncRandomTest, Generate128BitRandomBase64String) {
  std::string random_string = Generate128BitRandomBase64String();
  EXPECT_EQ(24U, random_string.size());
  char accumulator = 0;
  for (size_t i = 0; i < random_string.size(); ++i)
    accumulator |= random_string[i];
  // In theory this test can fail, but it won't before the universe dies of
  // heat death.
  EXPECT_NE(0, accumulator);
}
