// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

namespace {
const char* kHelloWorld = "hello world";
}  // namespace

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
  String s(kHelloWorld);
  EXPECT_EQ(std::string(kHelloWorld), s.get());
}

TEST(StringTest, Assignment) {
  String s(kHelloWorld);
  String t = s;  // Makes a copy.
  EXPECT_FALSE(t.is_null());
  EXPECT_EQ(std::string(kHelloWorld), t.get());
  EXPECT_FALSE(s.is_null());
}

TEST(StringTest, Equality) {
  String s(kHelloWorld);
  String t(kHelloWorld);
  EXPECT_EQ(s, t);
  EXPECT_TRUE(s == s);
  EXPECT_FALSE(s != s);
  EXPECT_TRUE(s == t);
  EXPECT_FALSE(s != t);
  EXPECT_TRUE(kHelloWorld == s);
  EXPECT_TRUE(s == kHelloWorld);
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

TEST(StringTest, MoveConstructors) {
  std::string std_str(kHelloWorld);

  String str1(std::move(std_str));
  EXPECT_TRUE(kHelloWorld == str1);

  String str2(std::move(str1));
  EXPECT_TRUE(kHelloWorld == str2);
  EXPECT_TRUE(str1.is_null());
}

TEST(StringTest, MoveAssignments) {
  std::string std_str(kHelloWorld);

  String str1;
  str1 = std::move(std_str);
  EXPECT_TRUE(kHelloWorld == str1);

  String str2;
  str2 = std::move(str1);
  EXPECT_TRUE(kHelloWorld == str2);
  EXPECT_TRUE(str1.is_null());
}

TEST(StringTest, Storage) {
  String str(kHelloWorld);

  EXPECT_TRUE(kHelloWorld == str.storage());

  std::string storage = str.PassStorage();
  EXPECT_TRUE(str.is_null());
  EXPECT_TRUE(kHelloWorld == storage);
}

}  // namespace test
}  // namespace mojo
