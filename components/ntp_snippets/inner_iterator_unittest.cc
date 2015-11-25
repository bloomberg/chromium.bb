// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "components/ntp_snippets/inner_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

int ii[] = {0, 1, 2, 3, 4};

class InnerIteratorTest : public testing::Test {
 protected:
  using ListType = std::vector<int*>;

  void SetUp() override {
    ints_.push_back(&ii[0]);
    ints_.push_back(&ii[1]);
    ints_.push_back(&ii[2]);
    ints_.push_back(&ii[3]);
    ints_.push_back(&ii[4]);
  }

  ListType ints_;
};

TEST_F(InnerIteratorTest, Create) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.begin());
  EXPECT_EQ(0, *x);
}
TEST_F(InnerIteratorTest, PreIncrement) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.begin());
  EXPECT_EQ(1, *(++x));
  EXPECT_EQ(1, *x);
}
TEST_F(InnerIteratorTest, PostIncrement) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.begin());
  EXPECT_EQ(0, *(x++));
  EXPECT_EQ(1, *x);
}
TEST_F(InnerIteratorTest, Add) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.begin());
  EXPECT_EQ(2, *(x + 2));
  EXPECT_EQ(0, *x);
}
TEST_F(InnerIteratorTest, Sub) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.end());
  EXPECT_EQ(2, *(x - 3));
  EXPECT_EQ(4, *(--x));
}
TEST_F(InnerIteratorTest, PreDecrement) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.end());
  EXPECT_EQ(4, *(--x));
  EXPECT_EQ(4, *x);
}
TEST_F(InnerIteratorTest, PostDecrement) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.end());
  EXPECT_EQ(4, *(--x));
  EXPECT_EQ(4, *(x--));
  EXPECT_EQ(3, *x);
}
TEST_F(InnerIteratorTest, Equality) {
  ntp_snippets::InnerIterator<ListType::iterator, int> x(ints_.begin());
  ntp_snippets::InnerIterator<ListType::iterator, int> y(ints_.end());
  x += 2;
  y -= 3;
  EXPECT_EQ(x, y);
}

}  // namespace
