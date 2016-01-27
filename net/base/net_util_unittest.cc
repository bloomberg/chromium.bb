// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include "base/format_macros.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // namespace net
