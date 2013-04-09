// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class StoragePartitionConfigTest : public testing::Test {
};

// Test that the Less comparison function is implemented properly to uniquely
// identify storage partitions used as keys in a std::map.
TEST_F(StoragePartitionConfigTest, OperatorLess) {
  StoragePartitionImplMap::StoragePartitionConfig c1(
      std::string(), std::string(), false);
  StoragePartitionImplMap::StoragePartitionConfig c2(
      std::string(), std::string(), false);
  StoragePartitionImplMap::StoragePartitionConfig c3(
      std::string(), std::string(), true);
  StoragePartitionImplMap::StoragePartitionConfig c4("a", std::string(), true);
  StoragePartitionImplMap::StoragePartitionConfig c5("b", std::string(), true);
  StoragePartitionImplMap::StoragePartitionConfig c6(
      std::string(), "abc", false);
  StoragePartitionImplMap::StoragePartitionConfig c7(
      std::string(), "abc", true);
  StoragePartitionImplMap::StoragePartitionConfig c8("a", "abc", false);
  StoragePartitionImplMap::StoragePartitionConfig c9("a", "abc", true);

  StoragePartitionImplMap::StoragePartitionConfigLess less;

  // Let's ensure basic comparison works.
  EXPECT_TRUE(less(c1, c3));
  EXPECT_TRUE(less(c1, c4));
  EXPECT_TRUE(less(c3, c4));
  EXPECT_TRUE(less(c4, c5));
  EXPECT_TRUE(less(c4, c8));
  EXPECT_TRUE(less(c6, c4));
  EXPECT_TRUE(less(c6, c7));
  EXPECT_TRUE(less(c8, c9));

  // Now, ensure antisymmetry for each pair we've tested.
  EXPECT_FALSE(less(c3, c1));
  EXPECT_FALSE(less(c4, c1));
  EXPECT_FALSE(less(c4, c3));
  EXPECT_FALSE(less(c5, c4));
  EXPECT_FALSE(less(c8, c4));
  EXPECT_FALSE(less(c4, c6));
  EXPECT_FALSE(less(c7, c6));
  EXPECT_FALSE(less(c9, c8));

  // Check for irreflexivity.
  EXPECT_FALSE(less(c1, c1));

  // Check for transitivity.
  EXPECT_TRUE(less(c1, c4));

  // Let's enforce that two identical elements obey strict weak ordering.
  EXPECT_TRUE(!less(c1, c2) && !less(c2, c1));
}

}  // namespace content
