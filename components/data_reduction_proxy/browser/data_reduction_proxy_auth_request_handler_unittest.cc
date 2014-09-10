// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kChromeProxyHeader[] = "chrome-proxy";
const char kOtherProxy[] = "testproxy:17";


#if defined(OS_ANDROID)
  const char kClient[] = "android";
#elif defined(OS_IOS)
  const char kClient[] = "ios";
#else
  const char kClient[] = "";
#endif
const char kVersion[] = "0.1.2.3";
const char kExpectedBuild[] = "2";
const char kExpectedPatch[] = "3";
const char kBogusVersion[] = "0.0";
const char kTestKey[] = "test-key";
const char kExpectedCredentials[] = "96bd72ec4a050ba60981743d41787768";
const char kExpectedSession[] = "0-1633771873-1633771873-1633771873";

const char kTestKey2[] = "test-key2";
const char kExpectedCredentials2[] = "c911fdb402f578787562cf7f00eda972";
const char kExpectedSession2[] = "0-1633771873-1633771873-1633771873";
#if defined(OS_ANDROID)
const char kExpectedHeader2[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, b=2, p=3, c=android";
const char kExpectedHeader3[] =
    "ps=86401-1633771873-1633771873-1633771873, "
    "sid=d7c1c34ef6b90303b01c48a6c1db6419, b=2, p=3, c=android";
const char kExpectedHeader4[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, c=android";
#elif defined(OS_IOS)
const char kExpectedHeader2[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, b=2, p=3, c=ios";
const char kExpectedHeader3[] =
    "ps=86401-1633771873-1633771873-1633771873, "
    "sid=d7c1c34ef6b90303b01c48a6c1db6419, b=2, p=3, c=ios";
const char kExpectedHeader4[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, c=ios";
#else
const char kExpectedHeader2[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, b=2, p=3";
const char kExpectedHeader3[] =
    "ps=86401-1633771873-1633771873-1633771873, "
    "sid=d7c1c34ef6b90303b01c48a6c1db6419, b=2, p=3";
const char kExpectedHeader4[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972";
#endif

const char kDataReductionProxyKey[] = "12345";
}  // namespace


namespace data_reduction_proxy {
namespace {
class TestDataReductionProxyAuthRequestHandler
    : public DataReductionProxyAuthRequestHandler {
 public:
  TestDataReductionProxyAuthRequestHandler(
      const std::string& client,
      const std::string& version,
      DataReductionProxyParams* params,
      base::MessageLoopProxy* loop_proxy)
      : DataReductionProxyAuthRequestHandler(
            client, version, params, loop_proxy) {}

  virtual std::string GetDefaultKey() const OVERRIDE {
    return kTestKey;
  }

  virtual base::Time Now() const OVERRIDE {
    return base::Time::UnixEpoch() + now_offset_;
  }

  virtual void RandBytes(void* output, size_t length) OVERRIDE {
    char* c =  static_cast<char*>(output);
    for (size_t i = 0; i < length; ++i) {
      c[i] = 'a';
    }
  }

  // Time after the unix epoch that Now() reports.
  void set_offset(const base::TimeDelta& now_offset) {
    now_offset_ = now_offset;
  }

 private:
  base::TimeDelta now_offset_;
};

}  // namespace

class DataReductionProxyAuthRequestHandlerTest : public testing::Test {
 public:
  DataReductionProxyAuthRequestHandlerTest()
      : loop_proxy_(base::MessageLoopProxy::current().get()) {
  }
  // Required for MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  base::MessageLoopProxy* loop_proxy_;
};

TEST_F(DataReductionProxyAuthRequestHandlerTest, Authorization) {
  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  TestDataReductionProxyAuthRequestHandler auth_handler(kClient,
                                                        kVersion,
                                                        params.get(),
                                                        loop_proxy_);
  auth_handler.Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(auth_handler.client_, kClient);
  EXPECT_EQ(kExpectedBuild, auth_handler.build_number_);
  EXPECT_EQ(kExpectedPatch, auth_handler.patch_number_);
  EXPECT_EQ(auth_handler.key_, kTestKey);
  EXPECT_EQ(kExpectedCredentials, auth_handler.credentials_);
  EXPECT_EQ(kExpectedSession, auth_handler.session_);

  // Now set a key.
  auth_handler.SetKeyOnUI(kTestKey2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey2, auth_handler.key_);
  EXPECT_EQ(kExpectedCredentials2, auth_handler.credentials_);
  EXPECT_EQ(kExpectedSession2, auth_handler.session_);

  // Don't write headers if the proxy is invalid.
  net::HttpRequestHeaders headers;
  auth_handler.MaybeAddRequestHeader(NULL, net::ProxyServer(), &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Don't write headers with a valid proxy, that's not a data reduction proxy.
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(kOtherProxy, net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Don't write headers with a valid data reduction ssl proxy.
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          net::HostPortPair::FromURL(
              GURL(params->DefaultSSLOrigin())).ToString(),
          net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Write headers with a valid data reduction proxy.
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          net::HostPortPair::FromURL(GURL(params->DefaultOrigin())).ToString(),
          net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader2, header_value);

  // Write headers with a valid data reduction ssl proxy when one is expected.
  net::HttpRequestHeaders ssl_headers;
  auth_handler.MaybeAddProxyTunnelRequestHandler(
      net::HostPortPair::FromURL(GURL(params->DefaultSSLOrigin())),
      &ssl_headers);
  EXPECT_TRUE(ssl_headers.HasHeader(kChromeProxyHeader));
  std::string ssl_header_value;
  ssl_headers.GetHeader(kChromeProxyHeader, &ssl_header_value);
  EXPECT_EQ(kExpectedHeader2, ssl_header_value);

  // Fast forward 24 hours. The header should be the same.
  auth_handler.set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60));
  net::HttpRequestHeaders headers2;
  // Write headers with a valid data reduction proxy.
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          net::HostPortPair::FromURL(GURL(params->DefaultOrigin())).ToString(),
          net::ProxyServer::SCHEME_HTTP),
      &headers2);
  EXPECT_TRUE(headers2.HasHeader(kChromeProxyHeader));
  std::string header_value2;
  headers2.GetHeader(kChromeProxyHeader, &header_value2);
  EXPECT_EQ(kExpectedHeader2, header_value2);

  // Fast forward one more second. The header should be new.
  auth_handler.set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60 + 1));
  net::HttpRequestHeaders headers3;
  // Write headers with a valid data reduction proxy.
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          net::HostPortPair::FromURL(GURL(params->DefaultOrigin())).ToString(),
          net::ProxyServer::SCHEME_HTTP),
      &headers3);
  EXPECT_TRUE(headers3.HasHeader(kChromeProxyHeader));
  std::string header_value3;
  headers3.GetHeader(kChromeProxyHeader, &header_value3);
  EXPECT_EQ(kExpectedHeader3, header_value3);
}

TEST_F(DataReductionProxyAuthRequestHandlerTest, AuthorizationBogusVersion) {
  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  TestDataReductionProxyAuthRequestHandler auth_handler(kClient,
                                                        kBogusVersion,
                                                        params.get(),
                                                        loop_proxy_);
  EXPECT_TRUE(auth_handler.build_number_.empty());
  EXPECT_TRUE(auth_handler.patch_number_.empty());

  // Now set a key.
  auth_handler.SetKeyOnUI(kTestKey2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey2, auth_handler.key_);
  EXPECT_EQ(kExpectedCredentials2, auth_handler.credentials_);
  EXPECT_EQ(kExpectedSession2, auth_handler.session_);

  net::HttpRequestHeaders headers;
  // Write headers with a valid data reduction proxy;
  auth_handler.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          net::HostPortPair::FromURL(GURL(params->DefaultOrigin())).ToString(),
          net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader4, header_value);
}

TEST_F(DataReductionProxyAuthRequestHandlerTest, AuthHashForSalt) {
  std::string salt = "8675309"; // Jenny's number to test the hash generator.
  std::string salted_key = salt + kDataReductionProxyKey + salt;
  base::string16 expected_hash = base::UTF8ToUTF16(base::MD5String(salted_key));
  EXPECT_EQ(expected_hash,
            DataReductionProxyAuthRequestHandler::AuthHashForSalt(
                8675309, kDataReductionProxyKey));
}

}  // namespace data_reduction_proxy
