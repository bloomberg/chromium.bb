// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_channel_api.h"

#include "base/memory/scoped_ptr.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {
namespace api {
namespace cast_channel {

// Tests URL parsing and validation.
TEST(CastChannelOpenFunctionTest, TestParseChannelUrl) {
  typedef CastChannelOpenFunction ccof;
  ConnectInfo connect_info;

  EXPECT_TRUE(ccof::ParseChannelUrl(GURL("cast://192.0.0.1:8009"),
                                    &connect_info));
  EXPECT_EQ(connect_info.ip_address, "192.0.0.1");
  EXPECT_EQ(connect_info.port, 8009);
  EXPECT_EQ(connect_info.auth, CHANNEL_AUTH_TYPE_SSL);

  EXPECT_TRUE(ccof::ParseChannelUrl(GURL("casts://192.0.0.1:12345"),
                                    &connect_info));
  EXPECT_EQ(connect_info.ip_address, "192.0.0.1");
  EXPECT_EQ(connect_info.port, 12345);
  EXPECT_EQ(connect_info.auth, CHANNEL_AUTH_TYPE_SSL_VERIFIED);

  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("http://192.0.0.1:12345"),
                                     &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast:192.0.0.1:12345"),
                                     &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast://:12345"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast://192.0.0.1:abcd"),
                                     &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL(""), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("foo"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast:"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast::"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast://192.0.0.1"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast://:"), &connect_info));
  EXPECT_FALSE(ccof::ParseChannelUrl(GURL("cast://192.0.0.1:"), &connect_info));
}

// Tests parsing of ConnectInfo.
TEST(CastChannelOpenFunctionTest, TestParseConnectInfo) {
  typedef CastChannelOpenFunction ccof;
  scoped_ptr<net::IPEndPoint> ip_endpoint;

  // Valid ConnectInfo
  ConnectInfo connect_info;
  connect_info.ip_address = "192.0.0.1";
  connect_info.port = 8009;
  connect_info.auth = CHANNEL_AUTH_TYPE_SSL;

  ip_endpoint.reset(ccof::ParseConnectInfo(connect_info));
  EXPECT_TRUE(ip_endpoint.get() != NULL);
  EXPECT_EQ(ip_endpoint->ToString(), "192.0.0.1:8009");
}

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions
