// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "chrome/test/webdriver/utility_functions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RandomIDTest, CanGenerateSufficientlyRandomIDs) {
  std::set<std::string> generated_ids;
  for (int i = 0; i < 10000; ++i) {
    std::string id = webdriver::GenerateRandomID();
    ASSERT_EQ(32u, id.length());
    ASSERT_TRUE(generated_ids.end() == generated_ids.find(id))
        << "Generated duplicate ID: " << id
        << " on iteration " << i;
    generated_ids.insert(id);
  }
}
