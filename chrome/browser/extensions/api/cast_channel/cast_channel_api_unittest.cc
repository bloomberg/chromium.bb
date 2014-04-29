// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cast_channel/cast_channel_api.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

base::ListValue* parseList(const std::string& json) {
  base::ListValue* result = NULL;
  base::Value* parsed = base::JSONReader::Read(json);
  CHECK(parsed);
  CHECK(parsed->GetAsList(&result));
  CHECK(result);
  return result;
}

}  // namespace

namespace extensions {

// Tests URL parsing and validation.
TEST(CastChannelOpenFunctionTest, TestParseChannelUrl) {
  typedef CastChannelOpenFunction ccof;
  cast_channel::ConnectInfo connect_info;

  EXPECT_TRUE(ccof::ParseChannelUrl(GURL("cast://192.0.0.1:8009"),
                                    &connect_info));
  EXPECT_EQ(connect_info.ip_address, "192.0.0.1");
  EXPECT_EQ(connect_info.port, 8009);
  EXPECT_EQ(connect_info.auth, cast_channel::CHANNEL_AUTH_TYPE_SSL);

  EXPECT_TRUE(ccof::ParseChannelUrl(GURL("casts://192.0.0.1:12345"),
                                    &connect_info));
  EXPECT_EQ(connect_info.ip_address, "192.0.0.1");
  EXPECT_EQ(connect_info.port, 12345);
  EXPECT_EQ(connect_info.auth, cast_channel::CHANNEL_AUTH_TYPE_SSL_VERIFIED);

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

// Tests validation of ConnectInfo.
TEST(CastChannelOpenFunctionTest, TestParseConnectInfo) {
  typedef CastChannelOpenFunction ccof;
  scoped_ptr<net::IPEndPoint> ip_endpoint;

  // Valid ConnectInfo
  cast_channel::ConnectInfo connect_info;
  connect_info.ip_address = "192.0.0.1";
  connect_info.port = 8009;
  connect_info.auth = cast_channel::CHANNEL_AUTH_TYPE_SSL;

  ip_endpoint.reset(ccof::ParseConnectInfo(connect_info));
  EXPECT_TRUE(ip_endpoint.get() != NULL);
  EXPECT_EQ(ip_endpoint->ToString(), "192.0.0.1:8009");

  // Invalid IP
  cast_channel::ConnectInfo invalid_ip_connect_info;
  invalid_ip_connect_info.ip_address = "blargh";
  invalid_ip_connect_info.port = 8009;
  invalid_ip_connect_info.auth = cast_channel::CHANNEL_AUTH_TYPE_SSL;
  ip_endpoint.reset(ccof::ParseConnectInfo(invalid_ip_connect_info));
  EXPECT_TRUE(ip_endpoint.get() == NULL);

  // Invalid port
  cast_channel::ConnectInfo invalid_port_connect_info;
  invalid_port_connect_info.ip_address = "192.0.0.1";
  invalid_port_connect_info.port = -1;
  invalid_port_connect_info.auth = cast_channel::CHANNEL_AUTH_TYPE_SSL;
  ip_endpoint.reset(ccof::ParseConnectInfo(invalid_port_connect_info));
  EXPECT_TRUE(ip_endpoint.get() == NULL);

  // Invalid auth
  cast_channel::ConnectInfo invalid_auth_connect_info;
  invalid_auth_connect_info.ip_address = "192.0.0.1";
  invalid_auth_connect_info.port = 8009;
  invalid_auth_connect_info.auth = cast_channel::CHANNEL_AUTH_TYPE_NONE;
  ip_endpoint.reset(ccof::ParseConnectInfo(invalid_auth_connect_info));
  EXPECT_TRUE(ip_endpoint.get() == NULL);
}

TEST(CastChannelOpenFunctionTest, TestPrepareValidUrl) {
  scoped_refptr<CastChannelOpenFunction> ccof(new CastChannelOpenFunction);
  scoped_ptr<base::ListValue> params_json(
      parseList("[\"casts://127.0.0.1:8009\"]"));

  ccof->SetArgs(params_json.get());
  EXPECT_TRUE(ccof->Prepare());
  EXPECT_TRUE(ccof->GetError().empty());
}

TEST(CastChannelOpenFunctionTest, TestPrepareInvalidUrl) {
  scoped_refptr<CastChannelOpenFunction> ccof(new CastChannelOpenFunction);
  scoped_ptr<base::ListValue> params_json(parseList("[\"blargh\"]"));

  ccof->SetArgs(params_json.get());
  EXPECT_FALSE(ccof->Prepare());
  EXPECT_EQ(ccof->GetError(), "Invalid Cast URL blargh");
}

TEST(CastChannelOpenFunctionTest, TestPrepareValidConnectInfo) {
  scoped_refptr<CastChannelOpenFunction> ccof(new CastChannelOpenFunction);
  scoped_ptr<base::ListValue> params_json(
      parseList("[{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
                "\"auth\": \"ssl\"}]"));

  ccof->SetArgs(params_json.get());
  EXPECT_TRUE(ccof->Prepare());
  EXPECT_TRUE(ccof->GetError().empty());
}

TEST(CastChannelOpenFunctionTest, TestPrepareInvalidConnectInfo) {
  scoped_refptr<CastChannelOpenFunction> ccof(new CastChannelOpenFunction);
  scoped_ptr<base::ListValue> params_json(parseList("[12345]"));

  ccof->SetArgs(params_json.get());
  EXPECT_FALSE(ccof->Prepare());
  EXPECT_EQ(ccof->GetError(), "Invalid connect_info");
}

TEST(CastChannelSendFunctionTest, TestPrepareValidMessageInfo) {
  scoped_refptr<CastChannelSendFunction> ccsf(new CastChannelSendFunction);
  scoped_ptr<base::ListValue> params_json(
      parseList("[{\"channelId\": 1, \"url\": \"cast://127.0.0.1:8009\", "
                "\"connectInfo\": "
                "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
                "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
                "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
                "\"destinationId\": \"bar\", \"data\": \"string\"}]"));

  ccsf->SetArgs(params_json.get());
  EXPECT_TRUE(ccsf->Prepare());
  EXPECT_TRUE(ccsf->GetError().empty());
}

TEST(CastChannelSendFunctionTest, TestPrepareInvalidMessageInfo) {
  scoped_refptr<CastChannelSendFunction> ccsf(new CastChannelSendFunction);
  scoped_ptr<base::ListValue> params_json(
      parseList("[{\"channelId\": 1, \"url\": \"cast://127.0.0.1:8009\", "
                "\"connectInfo\": "
                "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
                "\"auth\": \"ssl\"}, \"readyState\": \"open\"}, "
                "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
                "\"destinationId\": \"bar\", \"data\": 1235}]"));

  ccsf->SetArgs(params_json.get());
  EXPECT_FALSE(ccsf->Prepare());
  EXPECT_EQ(ccsf->GetError(), "Invalid type of message_info.data");
}

}  // namespace extensions
