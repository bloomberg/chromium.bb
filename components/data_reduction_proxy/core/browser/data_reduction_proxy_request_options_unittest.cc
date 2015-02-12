// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kChromeProxyHeader[] = "chrome-proxy";
const char kOtherProxy[] = "testproxy:17";

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
const char kDataReductionProxyKey[] = "12345";
}  // namespace


namespace data_reduction_proxy {
namespace {

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
const char kClientStr[] = "android";
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
const char kClientStr[] = "ios";
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
const char kClientStr[] = "mac";
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
const char kClientStr[] = "chromeos";
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
const char kClientStr[] = "linux";
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
const char kClientStr[] = "win";
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
const char kClientStr[] = "freebsd";
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
const char kClientStr[] = "openbsd";
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
const char kClientStr[] = "solaris";
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
const char kClientStr[] = "qnx";
#else
const Client kClient = Client::UNKNOWN;
const char kClientStr[] = "";
#endif

class TestDataReductionProxyRequestOptions
    : public DataReductionProxyRequestOptions {
 public:
  TestDataReductionProxyRequestOptions(
      Client client,
      const std::string& version,
      DataReductionProxyParams* params,
      base::MessageLoopProxy* loop_proxy)
      : DataReductionProxyRequestOptions(
            client, version, params, loop_proxy) {}

  std::string GetDefaultKey() const override { return kTestKey; }

  base::Time Now() const override {
    return base::Time::UnixEpoch() + now_offset_;
  }

  void RandBytes(void* output, size_t length) override {
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

class DataReductionProxyRequestOptionsTest : public testing::Test {
 public:
  DataReductionProxyRequestOptionsTest()
      : loop_proxy_(base::MessageLoopProxy::current().get()) {
  }

  std::map<std::string, std::string> ParseHeader(std::string header) {
    std::map<std::string, std::string> header_options;
    size_t pos = 0;
    std::string name;
    std::string value;
    std::string equals_delimiter = "=";
    std::string comma_delimiter = ", ";
    while ((pos = header.find(equals_delimiter)) != std::string::npos) {
        name = header.substr(0, pos);
        header.erase(0, pos + equals_delimiter.length());
        pos = header.find(comma_delimiter);
        if (pos != std::string::npos) {
          value = header.substr(0, pos);
          header.erase(0, pos + comma_delimiter.length());
          header_options[name] = value;
        }
    }

    if (!header.empty())
      header_options[name] = header;

    return header_options;
  }

  // Required for MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  base::MessageLoopProxy* loop_proxy_;
};

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationOnIOThread) {
  std::map<std::string, std::string> kExpectedHeader;
  kExpectedHeader[kSessionHeaderOption] = kExpectedSession2;
  kExpectedHeader[kCredentialsHeaderOption] = kExpectedCredentials2;
  kExpectedHeader[kBuildNumberHeaderOption] = "2";
  kExpectedHeader[kPatchNumberHeaderOption] =  "3";
  kExpectedHeader[kClientHeaderOption] = kClientStr;

  std::map<std::string, std::string> kExpectedHeader2;
  kExpectedHeader2[kSessionHeaderOption] =
      "86401-1633771873-1633771873-1633771873";
  kExpectedHeader2[kCredentialsHeaderOption] =
      "d7c1c34ef6b90303b01c48a6c1db6419";
  kExpectedHeader2[kBuildNumberHeaderOption] = "2";
  kExpectedHeader2[kPatchNumberHeaderOption] = "3";
  kExpectedHeader2[kClientHeaderOption] = kClientStr;

  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  // loop_proxy_ is just the current message loop. This means loop_proxy_
  // is the network thread used by DataReductionProxyRequestOptions.
  TestDataReductionProxyRequestOptions request_options(kClient,
                                                       kVersion,
                                                       params.get(),
                                                       loop_proxy_);
  request_options.Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kExpectedBuild,
            request_options.header_options_[kBuildNumberHeaderOption]);
  EXPECT_EQ(kExpectedPatch,
            request_options.header_options_[kPatchNumberHeaderOption]);
  EXPECT_EQ(request_options.key_, kTestKey);
  EXPECT_EQ(kExpectedCredentials,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession,
            request_options.header_options_[kSessionHeaderOption]);
  EXPECT_EQ(kClientStr, request_options.header_options_[kClientHeaderOption]);

  // Now set a key.
  request_options.SetKeyOnIO(kTestKey2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey2, request_options.key_);
  EXPECT_EQ(kExpectedCredentials2,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession2,
            request_options.header_options_[kSessionHeaderOption]);

  // Don't write headers if the proxy is invalid.
  net::HttpRequestHeaders headers;
  request_options.MaybeAddRequestHeader(NULL, net::ProxyServer(), &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Don't write headers with a valid proxy, that's not a data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(kOtherProxy, net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Don't write headers with a valid data reduction ssl proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultSSLOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));

  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader, ParseHeader(header_value));

  // Write headers with a valid data reduction ssl proxy when one is expected.
  net::HttpRequestHeaders ssl_headers;
  request_options.MaybeAddProxyTunnelRequestHandler(
      net::ProxyServer::FromURI(
        params->DefaultSSLOrigin(),
        net::ProxyServer::SCHEME_HTTP).host_port_pair(),
      &ssl_headers);
  EXPECT_TRUE(ssl_headers.HasHeader(kChromeProxyHeader));
  std::string ssl_header_value;
  ssl_headers.GetHeader(kChromeProxyHeader, &ssl_header_value);
  EXPECT_EQ(kExpectedHeader, ParseHeader(ssl_header_value));

  // Fast forward 24 hours. The header should be the same.
  request_options.set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60));
  net::HttpRequestHeaders headers2;
  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers2);
  EXPECT_TRUE(headers2.HasHeader(kChromeProxyHeader));
  std::string header_value2;
  headers2.GetHeader(kChromeProxyHeader, &header_value2);
  EXPECT_EQ(kExpectedHeader, ParseHeader(header_value2));

  // Fast forward one more second. The header should be new.
  request_options.set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60 + 1));
  net::HttpRequestHeaders headers3;
  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers3);
  EXPECT_TRUE(headers3.HasHeader(kChromeProxyHeader));
  std::string header_value3;
  headers3.GetHeader(kChromeProxyHeader, &header_value3);
  EXPECT_EQ(kExpectedHeader2, ParseHeader(header_value3));
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationIgnoresEmptyKey) {
scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  // loop_proxy_ is just the current message loop. This means loop_proxy_
  // is the network thread used by DataReductionProxyRequestOptions.
  TestDataReductionProxyRequestOptions request_options(kClient,
                                                       kVersion,
                                                       params.get(),
                                                       loop_proxy_);
  request_options.Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kExpectedBuild,
            request_options.header_options_[kBuildNumberHeaderOption]);
  EXPECT_EQ(kExpectedPatch,
            request_options.header_options_[kPatchNumberHeaderOption]);
  EXPECT_EQ(request_options.key_, kTestKey);
  EXPECT_EQ(kExpectedCredentials,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession,
            request_options.header_options_[kSessionHeaderOption]);
  EXPECT_EQ(kClientStr, request_options.header_options_[kClientHeaderOption]);

  // Now set an empty key. The auth handler should ignore that, and the key
  // remains |kTestKey|.
  request_options.SetKeyOnIO("");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(request_options.key_, kTestKey);
  EXPECT_EQ(kExpectedCredentials,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession,
            request_options.header_options_[kSessionHeaderOption]);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationBogusVersion) {
  std::map<std::string, std::string> kExpectedHeader;
  kExpectedHeader[kSessionHeaderOption] = kExpectedSession2;
  kExpectedHeader[kCredentialsHeaderOption] = kExpectedCredentials2;
  kExpectedHeader[kClientHeaderOption] = kClientStr;

  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));
  TestDataReductionProxyRequestOptions request_options(kClient,
                                                       kBogusVersion,
                                                       params.get(),
                                                       loop_proxy_);
  request_options.Init();
  EXPECT_TRUE(request_options.header_options_.find(kBuildNumberHeaderOption) ==
                  request_options.header_options_.end());
  EXPECT_TRUE(request_options.header_options_.find(kPatchNumberHeaderOption) ==
                  request_options.header_options_.end());

  // Now set a key.
  request_options.SetKeyOnIO(kTestKey2);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey2, request_options.key_);
  EXPECT_EQ(kExpectedCredentials2,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession2,
            request_options.header_options_[kSessionHeaderOption]);

  net::HttpRequestHeaders headers;
  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(
          params->DefaultOrigin(),
          net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader, ParseHeader(header_value));
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthHashForSalt) {
  std::string salt = "8675309"; // Jenny's number to test the hash generator.
  std::string salted_key = salt + kDataReductionProxyKey + salt;
  base::string16 expected_hash = base::UTF8ToUTF16(base::MD5String(salted_key));
  EXPECT_EQ(expected_hash,
            DataReductionProxyRequestOptions::AuthHashForSalt(
                8675309, kDataReductionProxyKey));
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationLoFi) {
  std::map<std::string, std::string> kExpectedHeader;
  kExpectedHeader[kSessionHeaderOption] = kExpectedSession;
  kExpectedHeader[kCredentialsHeaderOption] = kExpectedCredentials;
  kExpectedHeader[kClientHeaderOption] = kClientStr;
  kExpectedHeader[kLoFiHeaderOption] = "low";

  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);

  TestDataReductionProxyRequestOptions request_options(kClient,
                                                       kBogusVersion,
                                                       params.get(),
                                                       loop_proxy_);
  request_options.Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey, request_options.key_);
  EXPECT_EQ(kExpectedCredentials,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession,
            request_options.header_options_[kSessionHeaderOption]);

  net::HttpRequestHeaders headers;
  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader, ParseHeader(header_value));
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationLoFiOffThenOn) {
  std::map<std::string, std::string> kExpectedHeader;
  kExpectedHeader[kSessionHeaderOption] = kExpectedSession;
  kExpectedHeader[kCredentialsHeaderOption] = kExpectedCredentials;
  kExpectedHeader[kClientHeaderOption] = kClientStr;

  scoped_ptr<TestDataReductionProxyParams> params;
  params.reset(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN));

  TestDataReductionProxyRequestOptions request_options(kClient,
                                                       kBogusVersion,
                                                       params.get(),
                                                       loop_proxy_);
  request_options.Init();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(kTestKey, request_options.key_);
  EXPECT_EQ(kExpectedCredentials,
            request_options.header_options_[kCredentialsHeaderOption]);
  EXPECT_EQ(kExpectedSession,
            request_options.header_options_[kSessionHeaderOption]);

  net::HttpRequestHeaders headers;
  // Write headers with a valid data reduction proxy.
  request_options.MaybeAddRequestHeader(
      NULL,
      net::ProxyServer::FromURI(params->DefaultOrigin(),
                                net::ProxyServer::SCHEME_HTTP),
      &headers);
  EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
  std::string header_value;
  headers.GetHeader(kChromeProxyHeader, &header_value);
  EXPECT_EQ(kExpectedHeader, ParseHeader(header_value));

  // Add the LoFi command line switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);
  kExpectedHeader[kLoFiHeaderOption] = "low";

   // Write headers with a valid data reduction proxy.
   request_options.MaybeAddRequestHeader(
       NULL,
       net::ProxyServer::FromURI(params->DefaultOrigin(),
                                 net::ProxyServer::SCHEME_HTTP),
       &headers);
   EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
   headers.GetHeader(kChromeProxyHeader, &header_value);
   EXPECT_EQ(kExpectedHeader, ParseHeader(header_value));
}

}  // namespace data_reduction_proxy
