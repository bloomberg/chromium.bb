// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

TEST(StringTest, DefaultIsNotNull) {
  String s;
  EXPECT_FALSE(s.is_null());
}

TEST(StringTest, ConstructedWithNULL) {
  String s(nullptr);
  EXPECT_TRUE(s.is_null());
}

TEST(StringTest, ConstructedWithNullCharPointer) {
  const char* null = nullptr;
  String s(null);
  EXPECT_TRUE(s.is_null());
}

TEST(StringTest, AssignedNULL) {
  String s("");
  EXPECT_FALSE(s.is_null());
  s = nullptr;
  EXPECT_TRUE(s.is_null());
}

TEST(StringTest, Empty) {
  String s("");
  EXPECT_FALSE(s.is_null());
  EXPECT_TRUE(s.get().empty());
}

TEST(StringTest, Basic) {
  String s("hello world");
  EXPECT_EQ(std::string("hello world"), s.get());
}

TEST(StringTest, Assignment) {
  String s("hello world");
  String t = s;  // Makes a copy.
  EXPECT_FALSE(t.is_null());
  EXPECT_EQ(std::string("hello world"), t.get());
  EXPECT_FALSE(s.is_null());
}

TEST(StringTest, Equality) {
  String s("hello world");
  String t("hello world");
  EXPECT_EQ(s, t);
  EXPECT_TRUE(s == s);
  EXPECT_FALSE(s != s);
  EXPECT_TRUE(s == t);
  EXPECT_FALSE(s != t);
  EXPECT_TRUE("hello world" == s);
  EXPECT_TRUE(s == "hello world");
  EXPECT_TRUE("not" != s);
  EXPECT_FALSE("not" == s);
  EXPECT_TRUE(s != "not");
  EXPECT_FALSE(s == "not");

  // Test null strings.
  String n1;
  String n2;
  EXPECT_TRUE(n1 == n1);
  EXPECT_FALSE(n1 != n2);
  EXPECT_TRUE(n1 == n2);
  EXPECT_FALSE(n1 != n2);
  EXPECT_TRUE(n1 != s);
  EXPECT_FALSE(n1 == s);
  EXPECT_TRUE(s != n1);
  EXPECT_FALSE(s == n1);
}

TEST(StringTest, LessThanNullness) {
  String null;
  String null2;
  EXPECT_FALSE(null < null2);
  EXPECT_FALSE(null2 < null);

  String real("real");
  EXPECT_TRUE(null < real);
  EXPECT_FALSE(real < null);
}

}  // namespace test
}  // namespace mojo
