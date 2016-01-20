// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <ostream>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if !defined(OS_NACL) && !defined(OS_WIN)
#include <net/if.h>
#include <netinet/in.h>
#if defined(OS_MACOSX)
#include <ifaddrs.h>
#if !defined(OS_IOS)
#include <netinet/in_var.h>
#endif  // !OS_IOS
#endif  // OS_MACOSX
#endif  // !OS_NACL && !OS_WIN

#if !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_WIN)
#include "net/base/address_tracker_linux.h"
#endif  // !OS_MACOSX && !OS_NACL && !OS_WIN

namespace net {

namespace {

const unsigned char kLocalhostIPv4[] = {127, 0, 0, 1};
const unsigned char kLocalhostIPv6[] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
const uint16_t kLocalhostLookupPort = 80;

bool HasEndpoint(const IPEndPoint& endpoint, const AddressList& addresses) {
  for (const auto& address : addresses) {
    if (endpoint == address)
      return true;
  }
  return false;
}

void TestBothLoopbackIPs(const std::string& host) {
  IPEndPoint localhost_ipv4(
      IPAddressNumber(kLocalhostIPv4,
                      kLocalhostIPv4 + arraysize(kLocalhostIPv4)),
      kLocalhostLookupPort);
  IPEndPoint localhost_ipv6(
      IPAddressNumber(kLocalhostIPv6,
                      kLocalhostIPv6 + arraysize(kLocalhostIPv6)),
      kLocalhostLookupPort);

  AddressList addresses;
  EXPECT_TRUE(ResolveLocalHostname(host, kLocalhostLookupPort, &addresses));
  EXPECT_EQ(2u, addresses.size());
  EXPECT_TRUE(HasEndpoint(localhost_ipv4, addresses));
  EXPECT_TRUE(HasEndpoint(localhost_ipv6, addresses));
}

void TestIPv6LoopbackOnly(const std::string& host) {
  IPEndPoint localhost_ipv6(
      IPAddressNumber(kLocalhostIPv6,
                      kLocalhostIPv6 + arraysize(kLocalhostIPv6)),
      kLocalhostLookupPort);

  AddressList addresses;
  EXPECT_TRUE(ResolveLocalHostname(host, kLocalhostLookupPort, &addresses));
  EXPECT_EQ(1u, addresses.size());
  EXPECT_TRUE(HasEndpoint(localhost_ipv6, addresses));
}

}  // anonymous namespace

TEST(NetUtilTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = GetHostName();
  EXPECT_FALSE(hostname.empty());
}

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(IsLocalhost("localhost"));
  EXPECT_TRUE(IsLocalhost("localHosT"));
  EXPECT_TRUE(IsLocalhost("localhost."));
  EXPECT_TRUE(IsLocalhost("localHost."));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(IsLocalhost("localhost.localDOMain"));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain."));
  EXPECT_TRUE(IsLocalhost("localhost6"));
  EXPECT_TRUE(IsLocalhost("localhost6."));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6."));
  EXPECT_TRUE(IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(IsLocalhost("::1"));
  EXPECT_TRUE(IsLocalhost("0:0:0:0:0:0:0:1"));
  EXPECT_TRUE(IsLocalhost("foo.localhost"));
  EXPECT_TRUE(IsLocalhost("foo.localhost."));
  EXPECT_TRUE(IsLocalhost("foo.localhoST"));
  EXPECT_TRUE(IsLocalhost("foo.localhoST."));

  EXPECT_FALSE(IsLocalhost("localhostx"));
  EXPECT_FALSE(IsLocalhost("localhost.x"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain.x"));
  EXPECT_FALSE(IsLocalhost("localhost6x"));
  EXPECT_FALSE(IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(IsLocalhost("::2"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:0:0:0:0:1"));
  EXPECT_FALSE(IsLocalhost("foo.localhost.com"));
  EXPECT_FALSE(IsLocalhost("foo.localhoste"));
  EXPECT_FALSE(IsLocalhost("foo.localhos"));
}

TEST(NetUtilTest, ResolveLocalHostname) {
  AddressList addresses;

  TestBothLoopbackIPs("localhost");
  TestBothLoopbackIPs("localhoST");
  TestBothLoopbackIPs("localhost.");
  TestBothLoopbackIPs("localhoST.");
  TestBothLoopbackIPs("localhost.localdomain");
  TestBothLoopbackIPs("localhost.localdomAIn");
  TestBothLoopbackIPs("localhost.localdomain.");
  TestBothLoopbackIPs("localhost.localdomAIn.");
  TestBothLoopbackIPs("foo.localhost");
  TestBothLoopbackIPs("foo.localhOSt");
  TestBothLoopbackIPs("foo.localhost.");
  TestBothLoopbackIPs("foo.localhOSt.");

  TestIPv6LoopbackOnly("localhost6");
  TestIPv6LoopbackOnly("localhoST6");
  TestIPv6LoopbackOnly("localhost6.");
  TestIPv6LoopbackOnly("localhost6.localdomain6");
  TestIPv6LoopbackOnly("localhost6.localdomain6.");

  EXPECT_FALSE(
      ResolveLocalHostname("127.0.0.1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhostx", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhost.x", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain.x", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhost6x", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomain6",
                                    kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.localdomain",
                                    kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("127.0.0.1.1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname(".127.0.0.255", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::2", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:1:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localhost.com", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("foo.localhoste", kLocalhostLookupPort, &addresses));
}

struct NonUniqueNameTestData {
  bool is_unique;
  const char* const hostname;
};

// Google Test pretty-printer.
void PrintTo(const NonUniqueNameTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname);
  *os << " hostname: " << testing::PrintToString(data.hostname)
      << "; is_unique: " << testing::PrintToString(data.is_unique);
}

const NonUniqueNameTestData kNonUniqueNameTestData[] = {
    // Domains under ICANN-assigned domains.
    { true, "google.com" },
    { true, "google.co.uk" },
    // Domains under private registries.
    { true, "appspot.com" },
    { true, "test.appspot.com" },
    // Unreserved IPv4 addresses (in various forms).
    { true, "8.8.8.8" },
    { true, "99.64.0.0" },
    { true, "212.15.0.0" },
    { true, "212.15" },
    { true, "212.15.0" },
    { true, "3557752832" },
    // Reserved IPv4 addresses (in various forms).
    { false, "192.168.0.0" },
    { false, "192.168.0.6" },
    { false, "10.0.0.5" },
    { false, "10.0" },
    { false, "10.0.0" },
    { false, "3232235526" },
    // Unreserved IPv6 addresses.
    { true, "FFC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    { true, "2000:ba98:7654:2301:EFCD:BA98:7654:3210" },
    // Reserved IPv6 addresses.
    { false, "::192.9.5.5" },
    { false, "FEED::BEEF" },
    { false, "FEC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    // 'internal'/non-IANA assigned domains.
    { false, "intranet" },
    { false, "intranet." },
    { false, "intranet.example" },
    { false, "host.intranet.example" },
    // gTLDs under discussion, but not yet assigned.
    { false, "intranet.corp" },
    { false, "intranet.internal" },
    // Invalid host names are treated as unique - but expected to be
    // filtered out before then.
    { true, "junk)(£)$*!@~#" },
    { true, "w$w.example.com" },
    { true, "nocolonsallowed:example" },
    { true, "[::4.5.6.9]" },
};

class NetUtilNonUniqueNameTest
    : public testing::TestWithParam<NonUniqueNameTestData> {
 public:
  virtual ~NetUtilNonUniqueNameTest() {}

 protected:
  bool IsUnique(const std::string& hostname) {
    return !IsHostnameNonUnique(hostname);
  }
};

// Test that internal/non-unique names are properly identified as such, but
// that IP addresses and hosts beneath registry-controlled domains are flagged
// as unique names.
TEST_P(NetUtilNonUniqueNameTest, IsHostnameNonUnique) {
  const NonUniqueNameTestData& test_data = GetParam();

  EXPECT_EQ(test_data.is_unique, IsUnique(test_data.hostname));
}

INSTANTIATE_TEST_CASE_P(, NetUtilNonUniqueNameTest,
                        testing::ValuesIn(kNonUniqueNameTestData));

}  // namespace net
