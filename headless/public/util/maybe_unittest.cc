// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/maybe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {
namespace {

class MoveOnlyType {
 public:
  MoveOnlyType() {}
  MoveOnlyType(MoveOnlyType&& other) {}

  void operator=(MoveOnlyType&& other) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MoveOnlyType);
};

}  // namespace

TEST(MaybeTest, Nothing) {
  Maybe<int> maybe;
  EXPECT_TRUE(maybe.IsNothing());
  EXPECT_FALSE(maybe.IsJust());
}

TEST(MaybeTest, Just) {
  Maybe<int> maybe = Just(1);
  EXPECT_FALSE(maybe.IsNothing());
  EXPECT_TRUE(maybe.IsJust());
  EXPECT_EQ(1, maybe.FromJust());

  const Maybe<int> const_maybe = Just(2);
  EXPECT_EQ(2, const_maybe.FromJust());
}

TEST(MaybeTest, Equality) {
  Maybe<int> a;
  Maybe<int> b;
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);

  a = Just(1);
  EXPECT_NE(a, b);
  EXPECT_NE(b, a);

  b = Just(2);
  EXPECT_NE(a, b);
  EXPECT_NE(b, a);

  b = Just(1);
  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
}

TEST(MaybeTest, Assignment) {
  Maybe<int> a = Just(1);
  Maybe<int> b = Nothing<int>();
  EXPECT_NE(a, b);

  b = a;
  EXPECT_EQ(a, b);
}

TEST(MaybeTest, MoveOnlyType) {
  MoveOnlyType value;
  Maybe<MoveOnlyType> a = Just(std::move(value));
  EXPECT_TRUE(a.IsJust());

  Maybe<MoveOnlyType> b = Just(MoveOnlyType());
  EXPECT_TRUE(b.IsJust());

  Maybe<MoveOnlyType> c = Nothing<MoveOnlyType>();
  c = std::move(a);
  EXPECT_TRUE(c.IsJust());

  MoveOnlyType d = std::move(b.FromJust());
}

}  // namespace headless
