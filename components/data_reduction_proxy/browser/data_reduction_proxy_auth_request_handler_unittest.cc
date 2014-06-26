// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"

#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
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

const char kTestKey[] = "test-key";
const char kVersion[] = "0";
const char kExpectedCredentials[] = "96bd72ec4a050ba60981743d41787768";
const char kExpectedSession[] = "0-1633771873-1633771873-1633771873";

const char kTestKey2[] = "test-key2";
const char kClient2[] = "foo";
const char kVersion2[] = "2";
const char kExpectedCredentials2[] = "c911fdb402f578787562cf7f00eda972";
const char kExpectedSession2[] = "0-1633771873-1633771873-1633771873";
const char kExpectedHeader2[] =
    "ps=0-1633771873-1633771873-1633771873, "
    "sid=c911fdb402f578787562cf7f00eda972, v=2, c=foo";

const char kDataReductionProxyKey[] = "12345";
}  // namespace


namespace data_reduction_proxy {
namespace {
class TestDataReductionProxyAuthRequestHandler
    : public DataReductionProxyAuthRequestHandler {
 public:
  TestDataReductionProxyAuthRequestHandler(
      DataReductionProxyParams* params)
      : DataReductionProxyAuthRequestHandler(params) {}

  virtual std::string GetDefaultKey() const OVERRIDE {
    return kTestKey;
  }

  virtual base::Time Now() const OVERRIDE {
    return base::Time::UnixEpoch();
  }

  virtual void RandBytes(void* output, size_t length) OVERRIDE {
    char* c =  static_cast<char*>(output);
    for (size_t i = 0; i < length; ++i) {
      c[i] = 'a';
    }
  }
};

}  // namespace

class DataReductionProxyAuthRequestHandlerTest : public testing::Test {
};

TEST_F(DataReductionProxyAuthRequestHandlerTest, Authorization) {
  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN));
  TestDataReductionProxyAuthRequestHandler auth_handler(params.get());
  auth_handler.Init();
#if defined(OS_ANDROID)
  EXPECT_EQ(auth_handler.client_, "android");
#elif defined(OS_IOS)
  EXPECT_EQ(auth_handler.client_, "ios");
#else
  EXPECT_EQ(auth_handler.client_, "");
#endif
  EXPECT_EQ(kVersion, auth_handler.version_);
  EXPECT_EQ(auth_handler.key_, kTestKey);
  EXPECT_EQ(kExpectedCredentials, auth_handler.credentials_);
  EXPECT_EQ(kExpectedSession, auth_handler.session_);

  // Now set a key.
  auth_handler.SetKey(kTestKey2, kClient2, kVersion2);
  EXPECT_EQ(kClient2, auth_handler.client_);
  EXPECT_EQ(kVersion2, auth_handler.version_);
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
  EXPECT_EQ(kExpectedHeader2, header_value);
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
