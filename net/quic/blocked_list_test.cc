// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/blocked_list.h"
#include "net/quic/quic_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template<>
struct hash<const int*> {
  std::size_t operator()(const int* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}
#endif

namespace net {
namespace test {
namespace {

class BlockedListTest : public ::testing::Test {
 protected:
  BlockedListTest() :
      item1_(0),
      item2_(0),
      item3_(0) {
  }

  BlockedList<const int*> list_;
  const int item1_;
  const int item2_;
  const int item3_;
};

TEST_F(BlockedListTest, BasicAdd) {
  list_.AddBlockedObject(&item1_);
  list_.AddBlockedObject(&item3_);
  list_.AddBlockedObject(&item2_);
  ASSERT_EQ(3, list_.NumObjects());
  ASSERT_FALSE(list_.IsEmpty());

  EXPECT_EQ(&item1_, list_.GetNextBlockedObject());
  EXPECT_EQ(&item3_, list_.GetNextBlockedObject());
  EXPECT_EQ(&item2_, list_.GetNextBlockedObject());
}

TEST_F(BlockedListTest, AddAndRemove) {
  list_.AddBlockedObject(&item1_);
  list_.AddBlockedObject(&item3_);
  list_.AddBlockedObject(&item2_);
  ASSERT_EQ(3, list_.NumObjects());

  list_.RemoveBlockedObject(&item3_);
  ASSERT_EQ(2, list_.NumObjects());

  EXPECT_EQ(&item1_, list_.GetNextBlockedObject());
  EXPECT_EQ(&item2_, list_.GetNextBlockedObject());
}

TEST_F(BlockedListTest, DuplicateAdd) {
  list_.AddBlockedObject(&item1_);
  list_.AddBlockedObject(&item3_);
  list_.AddBlockedObject(&item2_);

  list_.AddBlockedObject(&item3_);
  list_.AddBlockedObject(&item2_);
  list_.AddBlockedObject(&item1_);

  ASSERT_EQ(3, list_.NumObjects());
  ASSERT_FALSE(list_.IsEmpty());

  // Call in the original insert order.
  EXPECT_EQ(&item1_, list_.GetNextBlockedObject());
  EXPECT_EQ(&item3_, list_.GetNextBlockedObject());
  EXPECT_EQ(&item2_, list_.GetNextBlockedObject());
}

}  // namespace
}  // namespace test
}  // namespace net
