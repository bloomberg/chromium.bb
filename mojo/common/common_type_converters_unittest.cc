// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_type_converters.h"

#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace mojo {
namespace common {
namespace test {

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

TEST(CommonTypeConvertersTest, ArrayUint8ToString16) {
  Array<uint8_t> data(8);
  data[0] = 'd';
  data[2] = 'a';
  data[4] = 't';
  data[6] = 'a';

  EXPECT_EQ(base::ASCIIToUTF16("data"), data.To<base::string16>());
}

TEST(CommonTypeConvertersTest, String16ToArrayUint8) {
  base::string16 input(base::ASCIIToUTF16("data"));
  Array<uint8_t> data = Array<uint8_t>::From(input);

  ASSERT_EQ(8ul, data.size());
  EXPECT_EQ('d', data[0]);
  EXPECT_EQ('a', data[2]);
  EXPECT_EQ('t', data[4]);
  EXPECT_EQ('a', data[6]);
}

TEST(CommonTypeConvertersTest, String16ToArrayUint8AndBack) {
  base::string16 input(base::ASCIIToUTF16("data"));
  Array<uint8_t> data = Array<uint8_t>::From(input);
  EXPECT_EQ(input, data.To<base::string16>());
}

TEST(CommonTypeConvertersTest, EmptyStringToArrayUint8) {
  Array<uint8_t> data = Array<uint8_t>::From(std::string());

  ASSERT_EQ(0ul, data.size());
  EXPECT_FALSE(data.is_null());
}

}  // namespace test
}  // namespace common
}  // namespace mojo
