// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/item_position.h"

#include "testing/gtest/include/gtest/gtest.h"

using enhanced_bookmarks::ItemPosition;

namespace {

class ItemPositionTest : public testing::Test {};

TEST_F(ItemPositionTest, TestCreateBefore) {
  ItemPosition current = ItemPosition::CreateInitialPosition();
  for (int i = 0; i < 10000; i++) {
    ItemPosition next = ItemPosition::CreateBefore(current);
    EXPECT_LT(next.ToString(), current.ToString());
    current = next;
  }
  // Make sure string lengths stay reasonable.
  EXPECT_LT(current.ToString().size(), 20u);
}

TEST_F(ItemPositionTest, TestCreateAfter) {
  ItemPosition current = ItemPosition::CreateInitialPosition();
  for (int i = 0; i < 10000; i++) {
    ItemPosition next = ItemPosition::CreateAfter(current);
    EXPECT_GT(next.ToString(), current.ToString());
    current = next;
  }
  // Make sure string lengths stay reasonable.
  EXPECT_LT(current.ToString().size(), 20u);
}

TEST_F(ItemPositionTest, TestCreateBetweenLeftBias) {
  ItemPosition before = ItemPosition::CreateInitialPosition();
  ItemPosition after = ItemPosition::CreateAfter(before);
  for (int i = 0; i < 10000; i++) {
    ItemPosition next = ItemPosition::CreateBetween(before, after);
    EXPECT_GT(next.ToString(), before.ToString());
    EXPECT_LT(next.ToString(), after.ToString());
    after = next;
  }
  // Make sure string lengths stay reasonable.
  EXPECT_LT(after.ToString().size(), 20u);
}

TEST_F(ItemPositionTest, TestCreateBetweenRightBias) {
  ItemPosition before = ItemPosition::CreateInitialPosition();
  ItemPosition after = ItemPosition::CreateAfter(before);
  for (int i = 0; i < 10000; i++) {
    ItemPosition next = ItemPosition::CreateBetween(before, after);
    EXPECT_GT(next.ToString(), before.ToString());
    EXPECT_LT(next.ToString(), after.ToString());
    before = next;
  }
  // Make sure string lengths stay reasonable.
  EXPECT_LT(before.ToString().size(), 20u);
}

TEST_F(ItemPositionTest, TestCreateBetweenAlternating) {
  ItemPosition before = ItemPosition::CreateInitialPosition();
  ItemPosition after = ItemPosition::CreateAfter(before);
  for (int i = 0; i < 1000; i++) {
    ItemPosition next = ItemPosition::CreateBetween(before, after);
    EXPECT_GT(next.ToString(), before.ToString());
    EXPECT_LT(next.ToString(), after.ToString());
    if ((i & 1) == 1)
      before = next;
    else
      after = next;
  }
  // There's no way to keep the string length down for all possible insertion
  // scenarios, and this one should be fairly rare in practice. Still verify
  // that at least the growth is restricted to about n*log_2(kPositionBase).
  EXPECT_LT(before.ToString().size(), 200u);
}

}  // namespace
