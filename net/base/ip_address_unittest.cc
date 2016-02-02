// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_address.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Helper to stringize an IP number (used to define expectations).
std::string DumpIPAddress(const IPAddress& v) {
  std::string out;
  for (size_t i = 0; i < v.bytes().size(); ++i) {
    if (i != 0)
      out.append(",");
    out.append(base::UintToString(v.bytes()[i]));
  }
  return out;
}

TEST(IPAddressTest, IsIPVersion) {
  uint8_t addr1[4] = {192, 168, 0, 1};
  IPAddress ip_address1(addr1);
  EXPECT_TRUE(ip_address1.IsIPv4());
  EXPECT_FALSE(ip_address1.IsIPv6());

  uint8_t addr2[16] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address2(addr2);
  EXPECT_TRUE(ip_address2.IsIPv6());
  EXPECT_FALSE(ip_address2.IsIPv4());

  IPAddress ip_address3;
  EXPECT_FALSE(ip_address3.IsIPv6());
  EXPECT_FALSE(ip_address3.IsIPv4());
}

TEST(IPAddressTest, IsValid) {
  uint8_t addr1[4] = {192, 168, 0, 1};
  IPAddress ip_address1(addr1);
  EXPECT_TRUE(ip_address1.IsValid());
  EXPECT_FALSE(ip_address1.empty());

  uint8_t addr2[16] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address2(addr2);
  EXPECT_TRUE(ip_address2.IsValid());
  EXPECT_FALSE(ip_address2.empty());

  uint8_t addr3[5] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address3(addr3);
  EXPECT_FALSE(ip_address3.IsValid());
  EXPECT_FALSE(ip_address3.empty());

  IPAddress ip_address4;
  EXPECT_FALSE(ip_address4.IsValid());
  EXPECT_TRUE(ip_address4.empty());
}

TEST(IPAddressTest, IsZero) {
  uint8_t address1[4] = {};
  IPAddress zero_ipv4_address(address1);
  EXPECT_TRUE(zero_ipv4_address.IsZero());

  uint8_t address2[4] = {10};
  IPAddress non_zero_ipv4_address(address2);
  EXPECT_FALSE(non_zero_ipv4_address.IsZero());

  uint8_t address3[16] = {};
  IPAddress zero_ipv6_address(address3);
  EXPECT_TRUE(zero_ipv6_address.IsZero());

  uint8_t address4[16] = {10};
  IPAddress non_zero_ipv6_address(address4);
  EXPECT_FALSE(non_zero_ipv6_address.IsZero());

  IPAddress empty_address;
  EXPECT_FALSE(empty_address.IsZero());
}

TEST(IPAddressTest, ToString) {
  uint8_t addr1[4] = {0, 0, 0, 0};
  IPAddress ip_address1(addr1);
  EXPECT_EQ("0.0.0.0", ip_address1.ToString());

  uint8_t addr2[4] = {192, 168, 0, 1};
  IPAddress ip_address2(addr2);
  EXPECT_EQ("192.168.0.1", ip_address2.ToString());

  uint8_t addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address3(addr3);
  EXPECT_EQ("fedc:ba98::", ip_address3.ToString());
}

// Test that invalid IP literals fail to parse.
TEST(IPAddressTest, FromIPLiteral_FailParse) {
  IPAddress address;

  EXPECT_FALSE(IPAddress::FromIPLiteral("bad value", &address));
  EXPECT_FALSE(IPAddress::FromIPLiteral("bad:value", &address));
  EXPECT_FALSE(IPAddress::FromIPLiteral(std::string(), &address));
  EXPECT_FALSE(IPAddress::FromIPLiteral("192.168.0.1:30", &address));
  EXPECT_FALSE(IPAddress::FromIPLiteral("  192.168.0.1  ", &address));
  EXPECT_FALSE(IPAddress::FromIPLiteral("[::1]", &address));
}

// Test parsing an IPv4 literal.
TEST(IPAddressTest, FromIPLiteral_IPv4) {
  IPAddress address;
  EXPECT_TRUE(IPAddress::FromIPLiteral("192.168.0.1", &address));
  EXPECT_EQ("192,168,0,1", DumpIPAddress(address));
  EXPECT_EQ("192.168.0.1", address.ToString());
}

// Test parsing an IPv6 literal.
TEST(IPAddressTest, FromIPLiteral_IPv6) {
  IPAddress address;
  EXPECT_TRUE(IPAddress::FromIPLiteral("1:abcd::3:4:ff", &address));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPAddress(address));
  EXPECT_EQ("1:abcd::3:4:ff", address.ToString());
}

TEST(IPAddressTest, IsIPv4Mapped) {
  IPAddress ipv4_address;
  EXPECT_TRUE(IPAddress::FromIPLiteral("192.168.0.1", &ipv4_address));
  EXPECT_FALSE(ipv4_address.IsIPv4Mapped());

  IPAddress ipv6_address;
  EXPECT_TRUE(IPAddress::FromIPLiteral("::1", &ipv4_address));
  EXPECT_FALSE(ipv6_address.IsIPv4Mapped());

  IPAddress ipv4mapped_address;
  EXPECT_TRUE(IPAddress::FromIPLiteral("::ffff:0101:1", &ipv4mapped_address));
  EXPECT_TRUE(ipv4mapped_address.IsIPv4Mapped());
}

TEST(IPAddressTest, IsEqual) {
  IPAddress ip_address1;
  EXPECT_TRUE(IPAddress::FromIPLiteral("127.0.0.1", &ip_address1));
  IPAddress ip_address2;
  EXPECT_TRUE(IPAddress::FromIPLiteral("2001:db8:0::42", &ip_address2));
  IPAddress ip_address3;
  EXPECT_TRUE(IPAddress::FromIPLiteral("127.0.0.1", &ip_address3));

  EXPECT_FALSE(ip_address1 == ip_address2);
  EXPECT_TRUE(ip_address1 == ip_address3);
}

TEST(IPAddressTest, LessThan) {
  // IPv4 vs IPv6
  IPAddress ip_address1;
  EXPECT_TRUE(IPAddress::FromIPLiteral("127.0.0.1", &ip_address1));
  IPAddress ip_address2;
  EXPECT_TRUE(IPAddress::FromIPLiteral("2001:db8:0::42", &ip_address2));
  EXPECT_TRUE(ip_address1 < ip_address2);
  EXPECT_FALSE(ip_address2 < ip_address1);

  // Compare equivalent addresses.
  IPAddress ip_address3;
  EXPECT_TRUE(IPAddress::FromIPLiteral("127.0.0.1", &ip_address3));
  EXPECT_FALSE(ip_address1 < ip_address3);
  EXPECT_FALSE(ip_address3 < ip_address1);
}

}  // anonymous namespace

}  // namespace net
