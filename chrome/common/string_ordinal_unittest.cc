// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <algorithm>

namespace {

TEST(StringOrdinalTest, IsValid) {
  EXPECT_TRUE(StringOrdinal("b").IsValid());
  EXPECT_TRUE(StringOrdinal("ab").IsValid());

  EXPECT_FALSE(StringOrdinal("aa").IsValid());
  EXPECT_FALSE(StringOrdinal().IsValid());
}

TEST(StringOrdinalTest, LessThan) {
  StringOrdinal small_value("b");
  StringOrdinal middle_value("n");
  StringOrdinal big_value("z");

  EXPECT_TRUE(small_value.LessThan(middle_value));
  EXPECT_TRUE(small_value.LessThan(big_value));
  EXPECT_TRUE(middle_value.LessThan(big_value));

  EXPECT_FALSE(big_value.LessThan(small_value));
  EXPECT_FALSE(big_value.LessThan(middle_value));
  EXPECT_FALSE(middle_value.LessThan(small_value));
}

TEST(StringOrdinalTest, GreaterThan) {
  StringOrdinal small_value("b");
  StringOrdinal middle_value("n");
  StringOrdinal big_value("z");

  EXPECT_TRUE(big_value.GreaterThan(small_value));
  EXPECT_TRUE(big_value.GreaterThan(middle_value));
  EXPECT_TRUE(middle_value.GreaterThan(small_value));


  EXPECT_FALSE(small_value.GreaterThan(middle_value));
  EXPECT_FALSE(small_value.GreaterThan(big_value));
  EXPECT_FALSE(middle_value.GreaterThan(big_value));
}

// Tests the CreateBetween StringOrdinal function by calling
// on the small_value with the large_value as the  parameter and
// vice-versa.
void TestCreateBetween(StringOrdinal small_value,
                       StringOrdinal large_value,
                       const std::string& expected_result) {
  StringOrdinal result = small_value.CreateBetween(large_value);
  EXPECT_EQ(expected_result, result.ToString());

  result = large_value.CreateBetween(small_value);
  EXPECT_EQ(expected_result, result.ToString());
}

TEST(StringOrdinalTest, CreateBetweenSingleDigit) {
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("d"), "c");
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("e"), "c");
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("f"), "d");
  TestCreateBetween(StringOrdinal("c"), StringOrdinal("d"), "cn");
}

TEST(StringOrdinalTest, CreateBetweenDifferentLengths) {
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("bb"), "ban");
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("db"), "c");
  TestCreateBetween(StringOrdinal("bz"), StringOrdinal("c"), "bzn");
  TestCreateBetween(StringOrdinal("baaaaaab"), StringOrdinal("d"), "c");
  TestCreateBetween(StringOrdinal("baaaaaac"), StringOrdinal("d"), "c");
  TestCreateBetween(StringOrdinal("b"), StringOrdinal("daaaaaaab"), "c");
}

TEST(StringOrdinalTest, CreateBetweenOverflow) {
  TestCreateBetween(StringOrdinal("ab"), StringOrdinal("bb"), "ao");
  TestCreateBetween(StringOrdinal("bb"), StringOrdinal("cb"), "bo");
  TestCreateBetween(StringOrdinal("bbb"), StringOrdinal("bcb"), "bbo");
  TestCreateBetween(StringOrdinal("aab"), StringOrdinal("zzz"), "n");
  TestCreateBetween(StringOrdinal("yyy"), StringOrdinal("zzz"), "zml");
  TestCreateBetween(StringOrdinal("yab"), StringOrdinal("zzz"), "z");
  TestCreateBetween(StringOrdinal("aaz"), StringOrdinal("zzz"), "n");
  TestCreateBetween(StringOrdinal("nnnnz"), StringOrdinal("mmmmz"), "n");
}

TEST(StringOrdinalTest, CreateAfter) {
  StringOrdinal result = StringOrdinal("y").CreateAfter();
  EXPECT_EQ("yn", result.ToString());

  result = StringOrdinal("zy").CreateAfter();
  EXPECT_EQ("zyn", result.ToString());

  result = StringOrdinal("zzzy").CreateAfter();
  EXPECT_EQ("zzzyn", result.ToString());

  result = StringOrdinal("yy").CreateAfter();
  EXPECT_EQ("zl", result.ToString());

  result = StringOrdinal("yz").CreateAfter();
  EXPECT_EQ("z", result.ToString());

  result = StringOrdinal("z").CreateAfter();
  EXPECT_EQ("zm", result.ToString());
}

TEST(StringOrdinalTest, CreateBefore) {
  StringOrdinal result = StringOrdinal("b").CreateBefore();
  EXPECT_EQ("an", result.ToString());

  result = StringOrdinal("bb").CreateBefore();
  EXPECT_EQ("ao", result.ToString());

  result = StringOrdinal("bc").CreateBefore();
  EXPECT_EQ("ao", result.ToString());

  result = StringOrdinal("bd").CreateBefore();
  EXPECT_EQ("ap", result.ToString());
}

TEST(StringOrdinalTest, ToString) {
  StringOrdinal index("b");
  EXPECT_EQ(index.ToString(), "b");

  index = StringOrdinal("aab");
  EXPECT_EQ(index.ToString(), "aab");

  index = StringOrdinal("zzz");
  EXPECT_EQ(index.ToString(), "zzz");
}

TEST(StringOrdinalTest, StdSort) {
  std::vector<StringOrdinal> ordinals;

  ordinals.push_back(StringOrdinal("z"));
  ordinals.push_back(StringOrdinal("n"));
  ordinals.push_back(StringOrdinal("j"));
  ordinals.push_back(StringOrdinal("b"));

  std::sort(ordinals.begin(), ordinals.end(), StringOrdinalLessThan());

  EXPECT_EQ(ordinals[0].ToString(), "b");
  EXPECT_EQ(ordinals[1].ToString(), "j");
  EXPECT_EQ(ordinals[2].ToString(), "n");
  EXPECT_EQ(ordinals[3].ToString(), "z");
}

TEST(StringOrdinalTest, StdFind) {
  std::vector<StringOrdinal> ordinals;

  StringOrdinal b("b");
  ordinals.push_back(b);
  ordinals.push_back(StringOrdinal("z"));

  EXPECT_EQ(std::find(ordinals.begin(), ordinals.end(), b), ordinals.begin());
  EXPECT_EQ(std::find(ordinals.begin(), ordinals.end(), StringOrdinal("z")),
            ordinals.begin() + 1);
  EXPECT_EQ(std::find(ordinals.begin(), ordinals.end(), StringOrdinal("n")),
            ordinals.end());
}

TEST(StringOrdinalTest, EqualOrBothInvalid) {
  StringOrdinal valid = StringOrdinal::CreateInitialOrdinal();
  StringOrdinal invalid;

  EXPECT_TRUE(valid.EqualOrBothInvalid(valid));
  EXPECT_TRUE(invalid.EqualOrBothInvalid(invalid));

  EXPECT_FALSE(invalid.EqualOrBothInvalid(valid));
}

}  // namespace
