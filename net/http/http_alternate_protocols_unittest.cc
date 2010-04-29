// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// HttpAlternateProtocols is an in-memory data structure used for keeping track
// of which HTTP HostPortPairs have an alternate protocol that can be used
// instead of HTTP on a different port.

#include "net/http/http_alternate_protocols.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(HttpAlternateProtocols, Basic) {
  HttpAlternateProtocols alternate_protocols;
  HostPortPair test_host_port_pair;
  test_host_port_pair.host = "foo";
  test_host_port_pair.port = 80;
  EXPECT_FALSE(
      alternate_protocols.HasAlternateProtocolFor(test_host_port_pair));
  alternate_protocols.SetAlternateProtocolFor(
      test_host_port_pair, 443, HttpAlternateProtocols::NPN_SPDY_1);
  ASSERT_TRUE(alternate_protocols.HasAlternateProtocolFor(test_host_port_pair));
  const HttpAlternateProtocols::PortProtocolPair alternate =
      alternate_protocols.GetAlternateProtocolFor(test_host_port_pair);
  EXPECT_EQ(443, alternate.port);
  EXPECT_EQ(HttpAlternateProtocols::NPN_SPDY_1, alternate.protocol);
}

TEST(HttpAlternateProtocols, SetBroken) {
  HttpAlternateProtocols alternate_protocols;
  HostPortPair test_host_port_pair;
  test_host_port_pair.host = "foo";
  test_host_port_pair.port = 80;
  alternate_protocols.MarkBrokenAlternateProtocolFor(test_host_port_pair);
  ASSERT_TRUE(alternate_protocols.HasAlternateProtocolFor(test_host_port_pair));
  HttpAlternateProtocols::PortProtocolPair alternate =
      alternate_protocols.GetAlternateProtocolFor(test_host_port_pair);
  EXPECT_EQ(HttpAlternateProtocols::BROKEN, alternate.protocol);

  alternate_protocols.SetAlternateProtocolFor(
      test_host_port_pair,
      1234,
      HttpAlternateProtocols::NPN_SPDY_1);
  alternate = alternate_protocols.GetAlternateProtocolFor(test_host_port_pair);
  EXPECT_EQ(HttpAlternateProtocols::BROKEN, alternate.protocol)
      << "Second attempt should be ignored.";
}

}  // namespace
}  // namespace net
