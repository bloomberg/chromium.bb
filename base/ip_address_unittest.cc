// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ip_address.h"
#include "third_party/googletest/src/googlemock/include/gmock/gmock.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {

using ::testing::ElementsAreArray;

TEST(IPv4AddressTest, Constructors) {
  IPv4Address address1(std::array<uint8_t, 4>{{1, 2, 3, 4}});
  EXPECT_THAT(address1.bytes, ElementsAreArray({1, 2, 3, 4}));

  uint8_t x[] = {4, 3, 2, 1};
  IPv4Address address2(x);
  EXPECT_THAT(address2.bytes, ElementsAreArray(x));

  IPv4Address address3(&x[0]);
  EXPECT_THAT(address3.bytes, ElementsAreArray(x));

  IPv4Address address4(6, 5, 7, 9);
  EXPECT_THAT(address4.bytes, ElementsAreArray({6, 5, 7, 9}));

  IPv4Address address5(address4);
  EXPECT_THAT(address5.bytes, ElementsAreArray({6, 5, 7, 9}));

  address5 = address1;
  EXPECT_THAT(address5.bytes, ElementsAreArray({1, 2, 3, 4}));
}

TEST(IPv4AddressTest, Comparison) {
  IPv4Address address1;
  EXPECT_EQ(address1, address1);

  IPv4Address address2({4, 3, 2, 1});
  EXPECT_NE(address1, address2);

  IPv4Address address3({4, 3, 2, 1});
  EXPECT_EQ(address2, address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
}

TEST(IPv4AddressTest, Parse) {
  IPv4Address address;
  ASSERT_TRUE(IPv4Address::Parse("192.168.0.1", &address));
  EXPECT_THAT(address.bytes, ElementsAreArray({192, 168, 0, 1}));
}

TEST(IPv4AddressTest, ParseFailures) {
  IPv4Address address;

  EXPECT_FALSE(IPv4Address::Parse("192..0.1", &address))
      << "empty value should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse(".192.168.0.1", &address))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse(".192.168.1", &address))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("..192.168.0.1", &address))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("..192.1", &address))
      << "leading dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.168.0.1.", &address))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.168.1.", &address))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.168.1..", &address))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.168..", &address))
      << "trailing dot should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.x3.0.1", &address))
      << "non-digit character should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.3.1", &address))
      << "too few values should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("192.3.2.0.1", &address))
      << "too many values should fail to parse";
  EXPECT_FALSE(IPv4Address::Parse("1920.3.2.1", &address))
      << "value > 255 should fail to parse";
}

TEST(IPv6AddressTest, Constructors) {
  IPv6Address address1(std::array<uint8_t, 16>{
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});
  EXPECT_THAT(address1.bytes, ElementsAreArray({1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
                                                11, 12, 13, 14, 15, 16}));

  const uint8_t x[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  IPv6Address address2(x);
  EXPECT_THAT(address2.bytes, ElementsAreArray(x));

  IPv6Address address3(&x[0]);
  EXPECT_THAT(address3.bytes, ElementsAreArray(x));

  IPv6Address address4(16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1);
  EXPECT_THAT(address4.bytes, ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                8, 7, 6, 5, 4, 3, 2, 1}));

  IPv6Address address5(address4);
  EXPECT_THAT(address5.bytes, ElementsAreArray({16, 15, 14, 13, 12, 11, 10, 9,
                                                8, 7, 6, 5, 4, 3, 2, 1}));
}

TEST(IPv6AddressTest, Comparison) {
  IPv6Address address1;
  EXPECT_EQ(address1, address1);

  IPv6Address address2({16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  EXPECT_NE(address1, address2);

  IPv6Address address3({16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1});
  EXPECT_EQ(address2, address3);

  address2 = address1;
  EXPECT_EQ(address1, address2);
}

TEST(IPv6AddressTest, ParseBasic) {
  IPv6Address address;
  ASSERT_TRUE(
      IPv6Address::Parse("abcd:ef01:2345:6789:9876:5432:10FE:DBCA", &address));
  EXPECT_THAT(
      address.bytes,
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x10, 0xfe, 0xdb, 0xca}));
}

TEST(IPv6AddressTest, ParseDoubleColon) {
  IPv6Address address1;
  ASSERT_TRUE(
      IPv6Address::Parse("abcd:ef01:2345:6789:9876:5432::dbca", &address1));
  EXPECT_THAT(
      address1.bytes,
      ElementsAreArray({0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0x98,
                        0x76, 0x54, 0x32, 0x00, 0x00, 0xdb, 0xca}));
  IPv6Address address2;
  ASSERT_TRUE(IPv6Address::Parse("abcd::10fe:dbca", &address2));
  EXPECT_THAT(
      address2.bytes,
      ElementsAreArray({0xab, 0xcd, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  IPv6Address address3;
  ASSERT_TRUE(IPv6Address::Parse("::10fe:dbca", &address3));
  EXPECT_THAT(
      address3.bytes,
      ElementsAreArray({0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x10, 0xfe, 0xdb, 0xca}));

  IPv6Address address4;
  ASSERT_TRUE(IPv6Address::Parse("10fe:dbca::", &address4));
  EXPECT_THAT(
      address4.bytes,
      ElementsAreArray({0x10, 0xfe, 0xdb, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(IPv6AddressTest, SmallValues) {
  IPv6Address address1;
  ASSERT_TRUE(IPv6Address::Parse("::", &address1));
  EXPECT_THAT(
      address1.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));

  IPv6Address address2;
  ASSERT_TRUE(IPv6Address::Parse("::1", &address2));
  EXPECT_THAT(
      address2.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}));

  IPv6Address address3;
  ASSERT_TRUE(IPv6Address::Parse("::2:1", &address3));
  EXPECT_THAT(
      address3.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01}));
}

TEST(IPv6AddressTest, ParseFailures) {
  IPv6Address address;

  EXPECT_FALSE(IPv6Address::Parse(":abcd::dbca", &address))
      << "leading colon should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abcd::dbca:", &address))
      << "trailing colon should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abxd::1234", &address))
      << "non-hex digit should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abcd:1234", &address))
      << "too few values should fail to parse";
  EXPECT_FALSE(
      IPv6Address::Parse("a:b:c:d:e:f:0:1:2:3:4:5:6:7:8:9:a", &address))
      << "too many values should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abcd1::dbca", &address))
      << "value > 0xffff should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("::abcd::dbca", &address))
      << "multiple double colon should fail to parse";

  EXPECT_FALSE(IPv6Address::Parse(":::abcd::dbca", &address))
      << "leading triple colon should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abcd:::dbca", &address))
      << "triple colon should fail to parse";
  EXPECT_FALSE(IPv6Address::Parse("abcd:dbca:::", &address))
      << "trailing triple colon should fail to parse";
}

TEST(IPv6AddressTest, ParseThreeDigitValue) {
  IPv6Address address;
  ASSERT_TRUE(IPv6Address::Parse("::123", &address));
  EXPECT_THAT(
      address.bytes,
      ElementsAreArray({0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x23}));
}

}  // namespace openscreen
