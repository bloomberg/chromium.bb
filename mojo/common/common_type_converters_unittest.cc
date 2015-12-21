// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_type_converters.h"

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "mojo/common/url_type_converters.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace common {
namespace test {
namespace {

void ExpectEqualsStringPiece(const std::string& expected,
                             const base::StringPiece& str) {
  EXPECT_EQ(expected, str.as_string());
}

void ExpectEqualsMojoString(const std::string& expected,
                            const String& str) {
  EXPECT_EQ(expected, str.get());
}

void ExpectEqualsString16(const base::string16& expected,
                          const base::string16& actual) {
  EXPECT_EQ(expected, actual);
}

void ExpectEqualsMojoString(const base::string16& expected,
                            const String& str) {
  EXPECT_EQ(expected, str.To<base::string16>());
}

}  // namespace

TEST(CommonTypeConvertersTest, StringPiece) {
  std::string kText("hello world");

  base::StringPiece string_piece(kText);
  String mojo_string(String::From(string_piece));

  ExpectEqualsMojoString(kText, mojo_string);
  ExpectEqualsStringPiece(kText, mojo_string.To<base::StringPiece>());

  // Test implicit construction and conversion:
  ExpectEqualsMojoString(kText, String::From(string_piece));
  ExpectEqualsStringPiece(kText, mojo_string.To<base::StringPiece>());

  // Test null String:
  base::StringPiece empty_string_piece = String().To<base::StringPiece>();
  EXPECT_TRUE(empty_string_piece.empty());
}

TEST(CommonTypeConvertersTest, String16) {
  const base::string16 string16(base::ASCIIToUTF16("hello world"));
  const String mojo_string(String::From(string16));

  ExpectEqualsMojoString(string16, mojo_string);
  EXPECT_EQ(string16, mojo_string.To<base::string16>());

  // Test implicit construction and conversion:
  ExpectEqualsMojoString(string16, String::From(string16));
  ExpectEqualsString16(string16, mojo_string.To<base::string16>());

  // Test empty string conversion.
  ExpectEqualsMojoString(base::string16(), String::From(base::string16()));
}

TEST(CommonTypeConvertersTest, URL) {
  GURL url("mojo:foo");
  String mojo_string(String::From(url));

  ASSERT_EQ(url.spec(), mojo_string);
  EXPECT_EQ(url.spec(), mojo_string.To<GURL>().spec());
  EXPECT_EQ(url.spec(), String::From(url));

  GURL invalid = String().To<GURL>();
  ASSERT_TRUE(invalid.spec().empty());

  String string_from_invalid = String::From(invalid);
  EXPECT_FALSE(string_from_invalid.is_null());
  ASSERT_EQ(0U, string_from_invalid.size());
}

TEST(CommonTypeConvertersTest, ArrayUint8ToStdString) {
  Array<uint8_t> data(4);
  data[0] = 'd';
  data[1] = 'a';
  data[2] = 't';
  data[3] = 'a';

  EXPECT_EQ("data", data.To<std::string>());
}

TEST(CommonTypeConvertersTest, StdStringToArrayUint8) {
  std::string input("data");
  Array<uint8_t> data = Array<uint8_t>::From(input);

  ASSERT_EQ(4ul, data.size());
  EXPECT_EQ('d', data[0]);
  EXPECT_EQ('a', data[1]);
  EXPECT_EQ('t', data[2]);
  EXPECT_EQ('a', data[3]);
}

TEST(CommonTypeConvertersTest, EmptyStringToArrayUint8) {
  Array<uint8_t> data = Array<uint8_t>::From(std::string());

  ASSERT_EQ(0ul, data.size());
  EXPECT_FALSE(data.is_null());
}

}  // namespace test
}  // namespace common
}  // namespace mojo
