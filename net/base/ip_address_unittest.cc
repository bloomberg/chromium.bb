// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_address.h"

#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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

TEST(IPAddressTest, ConstructIPv4) {
  EXPECT_EQ("127.0.0.1", IPAddress::IPv4Localhost().ToString());

  IPAddress ipv4_ctor(192, 168, 1, 1);
  EXPECT_EQ("192.168.1.1", ipv4_ctor.ToString());
}

TEST(IPAddressTest, ConstructIPv6) {
  EXPECT_EQ("::1", IPAddress::IPv6Localhost().ToString());

  IPAddress ipv6_ctor(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16);
  EXPECT_EQ("102:304:506:708:90a:b0c:d0e:f10", ipv6_ctor.ToString());
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

TEST(IPAddressTest, IsReserved) {
  struct {
    const char* const address;
    bool is_reserved;
  } tests[] = {{"10.10.10.10", true},
               {"9.9.255.255", false},
               {"127.0.0.1", true},
               {"128.0.0.1", false},
               {"198.18.0.0", true},
               {"198.18.255.255", true},
               {"198.17.255.255", false},
               {"198.19.255.255", true},
               {"198.20.0.0", false},
               {"0000::", true},
               {"FFC0:ba98:7654:3210:FEDC:BA98:7654:3210", false},
               {"2000:ba98:7654:2301:EFCD:BA98:7654:3210", false},
               {"::192.9.5.5", true},
               {"FEED::BEEF", true}};

  IPAddress address;
  for (const auto& test : tests) {
    EXPECT_TRUE(address.AssignFromIPLiteral(test.address));
    EXPECT_EQ(test.is_reserved, address.IsReserved());
  }
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

TEST(IpAddressNumberTest, IsIPv4Mapped) {
  IPAddress ipv4_address(192, 168, 0, 1);
  EXPECT_FALSE(ipv4_address.IsIPv4MappedIPv6());
  IPAddress ipv6_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  EXPECT_FALSE(ipv6_address.IsIPv4MappedIPv6());
  IPAddress mapped_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 1, 1, 0, 1);
  EXPECT_TRUE(mapped_address.IsIPv4MappedIPv6());
}

TEST(IPAddressTest, AllZeros) {
  EXPECT_TRUE(IPAddress::AllZeros(0).empty());

  EXPECT_EQ(3u, IPAddress::AllZeros(3).size());
  EXPECT_TRUE(IPAddress::AllZeros(3).IsZero());

  EXPECT_EQ("0.0.0.0", IPAddress::IPv4AllZeros().ToString());
  EXPECT_EQ("::", IPAddress::IPv6AllZeros().ToString());
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

  // ToString() shouldn't crash on invalid addresses.
  uint8_t addr4[2];
  IPAddress ip_address4(addr4);
  EXPECT_EQ("", ip_address4.ToString());

  IPAddress ip_address5;
  EXPECT_EQ("", ip_address5.ToString());
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

TEST(IpAddressNumberTest, IsIPv4MappedIPv6) {
  IPAddress ipv4_address(192, 168, 0, 1);
  EXPECT_FALSE(ipv4_address.IsIPv4MappedIPv6());
  IPAddress ipv6_address = IPAddress::IPv6Localhost();
  EXPECT_FALSE(ipv6_address.IsIPv4MappedIPv6());
  IPAddress mapped_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 1, 1, 0, 1);
  EXPECT_TRUE(mapped_address.IsIPv4MappedIPv6());
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

  // ToString() shouldn't crash on invalid addresses.
  IPAddress address4;
  EXPECT_EQ("", IPAddressToStringWithPort(address4, 8080));
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

// Test mapping an IPv4 address to an IPv6 address.
TEST(IPAddressTest, ConvertIPv4ToIPv4MappedIPv6) {
  IPAddress ipv4_address(192, 168, 0, 1);
  IPAddress ipv6_address = ConvertIPv4ToIPv4MappedIPv6(ipv4_address);

  // ::ffff:192.168.0.1
  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPAddress(ipv6_address));
  EXPECT_EQ("::ffff:c0a8:1", ipv6_address.ToString());
}

// Test reversal of a IPv6 address mapping.
TEST(IPAddressTest, ConvertIPv4MappedIPv6ToIPv4) {
  IPAddress ipv4mapped_address;
  EXPECT_TRUE(ipv4mapped_address.AssignFromIPLiteral("::ffff:c0a8:1"));

  IPAddress expected(192, 168, 0, 1);

  IPAddress result = ConvertIPv4MappedIPv6ToIPv4(ipv4mapped_address);
  EXPECT_EQ(expected, result);
}

TEST(IPAddressTest, IPAddressMatchesPrefix) {
  struct {
    const char* const cidr_literal;
    size_t prefix_length_in_bits;
    const char* const ip_literal;
    bool expected_to_match;
  } tests[] = {
      // IPv4 prefix with IPv4 inputs.
      {"10.10.1.32", 27, "10.10.1.44", true},
      {"10.10.1.32", 27, "10.10.1.90", false},
      {"10.10.1.32", 27, "10.10.1.90", false},

      // IPv6 prefix with IPv6 inputs.
      {"2001:db8::", 32, "2001:DB8:3:4::5", true},
      {"2001:db8::", 32, "2001:c8::", false},

      // IPv6 prefix with IPv4 inputs.
      {"2001:db8::", 33, "192.168.0.1", false},
      {"::ffff:192.168.0.1", 112, "192.168.33.77", true},

      // IPv4 prefix with IPv6 inputs.
      {"10.11.33.44", 16, "::ffff:0a0b:89", true},
      {"10.11.33.44", 16, "::ffff:10.12.33.44", false},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s, %s", i,
                                    tests[i].cidr_literal,
                                    tests[i].ip_literal));

    IPAddress ip_address;
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(tests[i].ip_literal));

    IPAddress ip_prefix;
    EXPECT_TRUE(ip_prefix.AssignFromIPLiteral(tests[i].cidr_literal));

    EXPECT_EQ(tests[i].expected_to_match,
              IPAddressMatchesPrefix(ip_address, ip_prefix,
                                     tests[i].prefix_length_in_bits));
  }
}

// Test parsing invalid CIDR notation literals.
TEST(IPAddressTest, ParseCIDRBlock_Invalid) {
  const char* const bad_literals[] = {"foobar",
                                      "",
                                      "192.168.0.1",
                                      "::1",
                                      "/",
                                      "/1",
                                      "1",
                                      "192.168.1.1/-1",
                                      "192.168.1.1/33",
                                      "::1/-3",
                                      "a::3/129",
                                      "::1/x",
                                      "192.168.0.1//11",
                                      "192.168.1.1/+1",
                                      "192.168.1.1/ +1",
                                      "192.168.1.1/"};

  for (const auto& bad_literal : bad_literals) {
    IPAddress ip_address;
    size_t prefix_length_in_bits;

    EXPECT_FALSE(
        ParseCIDRBlock(bad_literal, &ip_address, &prefix_length_in_bits));
  }
}

// Test parsing a valid CIDR notation literal.
TEST(IPAddressTest, ParseCIDRBlock_Valid) {
  IPAddress ip_address;
  size_t prefix_length_in_bits;

  EXPECT_TRUE(
      ParseCIDRBlock("192.168.0.1/11", &ip_address, &prefix_length_in_bits));

  EXPECT_EQ("192,168,0,1", DumpIPAddress(ip_address));
  EXPECT_EQ(11u, prefix_length_in_bits);

  EXPECT_TRUE(ParseCIDRBlock("::ffff:192.168.0.1/112", &ip_address,
                             &prefix_length_in_bits));

  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPAddress(ip_address));
  EXPECT_EQ(112u, prefix_length_in_bits);
}

TEST(IPAddressTest, ParseURLHostnameToAddress_FailParse) {
  IPAddress address;
  EXPECT_FALSE(ParseURLHostnameToAddress("bad value", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("bad:value", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress(std::string(), &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("192.168.0.1:30", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("  192.168.0.1  ", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("::1", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("[192.169.0.1]", &address));
}

TEST(IPAddressTest, ParseURLHostnameToAddress_IPv4) {
  IPAddress address;
  EXPECT_TRUE(ParseURLHostnameToAddress("192.168.0.1", &address));
  EXPECT_EQ("192,168,0,1", DumpIPAddress(address));
  EXPECT_EQ("192.168.0.1", address.ToString());
}

TEST(IPAddressTest, ParseURLHostnameToAddress_IPv6) {
  IPAddress address;
  EXPECT_TRUE(ParseURLHostnameToAddress("[1:abcd::3:4:ff]", &address));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPAddress(address));
  EXPECT_EQ("1:abcd::3:4:ff", address.ToString());
}

TEST(IPAddressTest, IPAddressStartsWith) {
  IPAddress ipv4_address(192, 168, 10, 5);

  uint8_t ipv4_prefix1[] = {192, 168, 10};
  EXPECT_TRUE(IPAddressStartsWith(ipv4_address, ipv4_prefix1));

  uint8_t ipv4_prefix3[] = {192, 168, 10, 5};
  EXPECT_TRUE(IPAddressStartsWith(ipv4_address, ipv4_prefix3));

  uint8_t ipv4_prefix2[] = {192, 168, 10, 10};
  EXPECT_FALSE(IPAddressStartsWith(ipv4_address, ipv4_prefix2));

  // Prefix is longer than the address.
  uint8_t ipv4_prefix4[] = {192, 168, 10, 10, 0};
  EXPECT_FALSE(IPAddressStartsWith(ipv4_address, ipv4_prefix4));

  IPAddress ipv6_address;
  EXPECT_TRUE(ipv6_address.AssignFromIPLiteral("2a00:1450:400c:c09::64"));

  uint8_t ipv6_prefix1[] = {42, 0, 20, 80, 64, 12, 12, 9};
  EXPECT_TRUE(IPAddressStartsWith(ipv6_address, ipv6_prefix1));

  uint8_t ipv6_prefix2[] = {41, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  100};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix2));

  uint8_t ipv6_prefix3[] = {42, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  0,  100};
  EXPECT_TRUE(IPAddressStartsWith(ipv6_address, ipv6_prefix3));

  uint8_t ipv6_prefix4[] = {42, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  0,  0};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix4));

  // Prefix is longer than the address.
  uint8_t ipv6_prefix5[] = {42, 0, 20, 80, 64, 12, 12, 9, 0,
                            0,  0, 0,  0,  0,  0,  0,  10};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix5));
}

}  // anonymous namespace

}  // namespace net
