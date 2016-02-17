// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_address.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Helper to stringize an IP address (used to define expectations).
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
TEST(IPAddressTest, AssignFromIPLiteral_FailParse) {
  IPAddress address;

  EXPECT_FALSE(address.AssignFromIPLiteral("bad value"));
  EXPECT_FALSE(address.AssignFromIPLiteral("bad:value"));
  EXPECT_FALSE(address.AssignFromIPLiteral(std::string()));
  EXPECT_FALSE(address.AssignFromIPLiteral("192.168.0.1:30"));
  EXPECT_FALSE(address.AssignFromIPLiteral("  192.168.0.1  "));
  EXPECT_FALSE(address.AssignFromIPLiteral("[::1]"));
}

// Test parsing an IPv4 literal.
TEST(IPAddressTest, AssignFromIPLiteral_IPv4) {
  IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.0.1"));
  EXPECT_EQ("192,168,0,1", DumpIPAddress(address));
  EXPECT_EQ("192.168.0.1", address.ToString());
}

// Test parsing an IPv6 literal.
TEST(IPAddressTest, AssignFromIPLiteral_IPv6) {
  IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("1:abcd::3:4:ff"));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPAddress(address));
  EXPECT_EQ("1:abcd::3:4:ff", address.ToString());
}

TEST(IPAddressTest, IsIPv4MappedIPv6) {
  IPAddress ipv4_address;
  EXPECT_TRUE(ipv4_address.AssignFromIPLiteral("192.168.0.1"));
  EXPECT_FALSE(ipv4_address.IsIPv4MappedIPv6());

  IPAddress ipv6_address;
  EXPECT_TRUE(ipv6_address.AssignFromIPLiteral("::1"));
  EXPECT_FALSE(ipv6_address.IsIPv4MappedIPv6());

  IPAddress ipv4mapped_address;
  EXPECT_TRUE(ipv4mapped_address.AssignFromIPLiteral("::ffff:0101:1"));
  EXPECT_TRUE(ipv4mapped_address.IsIPv4MappedIPv6());
}

TEST(IPAddressTest, IsEqual) {
  IPAddress ip_address1;
  EXPECT_TRUE(ip_address1.AssignFromIPLiteral("127.0.0.1"));
  IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral("2001:db8:0::42"));
  IPAddress ip_address3;
  EXPECT_TRUE(ip_address3.AssignFromIPLiteral("127.0.0.1"));

  EXPECT_FALSE(ip_address1 == ip_address2);
  EXPECT_TRUE(ip_address1 == ip_address3);
}

TEST(IPAddressTest, LessThan) {
  // IPv4 vs IPv6
  IPAddress ip_address1;
  EXPECT_TRUE(ip_address1.AssignFromIPLiteral("127.0.0.1"));
  IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral("2001:db8:0::42"));
  EXPECT_TRUE(ip_address1 < ip_address2);
  EXPECT_FALSE(ip_address2 < ip_address1);

  // Compare equivalent addresses.
  IPAddress ip_address3;
  EXPECT_TRUE(ip_address3.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_FALSE(ip_address1 < ip_address3);
  EXPECT_FALSE(ip_address3 < ip_address1);
}

TEST(IPAddressTest, IPAddressToStringWithPort) {
  IPAddress address1;
  EXPECT_TRUE(address1.AssignFromIPLiteral("0.0.0.0"));
  EXPECT_EQ("0.0.0.0:3", IPAddressToStringWithPort(address1, 3));

  IPAddress address2;
  EXPECT_TRUE(address2.AssignFromIPLiteral("192.168.0.1"));
  EXPECT_EQ("192.168.0.1:99", IPAddressToStringWithPort(address2, 99));

  IPAddress address3;
  EXPECT_TRUE(address3.AssignFromIPLiteral("fedc:ba98::"));
  EXPECT_EQ("[fedc:ba98::]:8080", IPAddressToStringWithPort(address3, 8080));
}

TEST(IPAddressTest, IPAddressToPackedString) {
  IPAddress ipv4_address;
  EXPECT_TRUE(ipv4_address.AssignFromIPLiteral("4.31.198.44"));
  std::string expected_ipv4_address("\x04\x1f\xc6\x2c", 4);
  EXPECT_EQ(expected_ipv4_address, IPAddressToPackedString(ipv4_address));

  IPAddress ipv6_address;
  EXPECT_TRUE(ipv6_address.AssignFromIPLiteral("2001:0700:0300:1800::000f"));
  std::string expected_ipv6_address(
      "\x20\x01\x07\x00\x03\x00\x18\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x0f",
      16);
  EXPECT_EQ(expected_ipv6_address, IPAddressToPackedString(ipv6_address));
}

TEST(IPAddressTest, ConvertIPv4ToIPv4MappedIPv6) {
  IPAddress ipv4_address;
  EXPECT_TRUE(ipv4_address.AssignFromIPLiteral("192.168.0.1"));

  IPAddress ipv6_address = ConvertIPv4ToIPv4MappedIPv6(ipv4_address);

  // ::ffff:192.168.0.1
  EXPECT_EQ("::ffff:c0a8:1", ipv6_address.ToString());
}

TEST(IPAddressTest, ConvertIPv4MappedIPv6ToIPv4) {
  IPAddress ipv4mapped_address;
  EXPECT_TRUE(ipv4mapped_address.AssignFromIPLiteral("::ffff:c0a8:1"));

  IPAddress expected;
  EXPECT_TRUE(expected.AssignFromIPLiteral("192.168.0.1"));

  IPAddress result = ConvertIPv4MappedIPv6ToIPv4(ipv4mapped_address);
  EXPECT_EQ(expected, result);
}

}  // anonymous namespace

}  // namespace net
