// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
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
      DataReductionProxyConfig* config,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : DataReductionProxyRequestOptions(
            client, version, config, task_runner) {}

  std::string GetDefaultKey() const override {
    return kTestKey;
  }

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

void SetHeaderExpectations(const std::string& session,
                           const std::string& credentials,
                           const std::string& client,
                           const std::string& build,
                           const std::string& patch,
                           const std::string& lofi,
                           const std::vector<std::string> experiments,
                           std::string* expected_header) {
  std::vector<std::string> expected_options;
  if (!session.empty()) {
    expected_options.push_back(
        std::string(kSessionHeaderOption) + "=" + session);
  }
  if (!credentials.empty()) {
    expected_options.push_back(
        std::string(kCredentialsHeaderOption) + "=" + credentials);
  }
  if (!client.empty()) {
    expected_options.push_back(
        std::string(kClientHeaderOption) + "=" + client);
  }
  if (!build.empty()) {
    expected_options.push_back(
        std::string(kBuildNumberHeaderOption) + "=" + build);
  }
  if (!patch.empty()) {
    expected_options.push_back(
        std::string(kPatchNumberHeaderOption) + "=" + patch);
  }
  if (!lofi.empty()) {
    expected_options.push_back(
        std::string(kLoFiHeaderOption) + "=" + lofi);
  }
  for (const auto& experiment : experiments) {
    expected_options.push_back(
        std::string(kExperimentsOption) + "=" + experiment);
  }
  if (!expected_options.empty())
    *expected_header = JoinString(expected_options, ", ");
}

}  // namespace

class DataReductionProxyRequestOptionsTest : public testing::Test {
 public:
  DataReductionProxyRequestOptionsTest() {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                                 DataReductionProxyParams::kFallbackAllowed |
                                 DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                TestDataReductionProxyParams::HAS_EVERYTHING &
                    ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                    ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)
            .Build();
  }

  void CreateRequestOptions(const std::string& version) {
    request_options_.reset(
        new TestDataReductionProxyRequestOptions(
            kClient, version, test_context_->config(),
            test_context_->task_runner()));
    request_options_->Init();
  }

  TestDataReductionProxyParams* params() {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyRequestOptions* request_options() {
    return request_options_.get();
  }

  void VerifyExpectedHeader(const std::string& proxy_uri,
                            const std::string& expected_header) {
    test_context_->RunUntilIdle();
    net::HttpRequestHeaders headers;
    request_options_->MaybeAddRequestHeader(
        NULL,
        proxy_uri.empty() ? net::ProxyServer() :
            net::ProxyServer::FromURI(proxy_uri, net::ProxyServer::SCHEME_HTTP),
        &headers);
    if (expected_header.empty()) {
      EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));
      return;
    }
    EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
    std::string header_value;
    headers.GetHeader(kChromeProxyHeader, &header_value);
    EXPECT_EQ(expected_header, header_value);
  }

  scoped_ptr<TestDataReductionProxyRequestOptions> request_options_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
};

TEST_F(DataReductionProxyRequestOptionsTest, AuthHashForSalt) {
  std::string salt = "8675309"; // Jenny's number to test the hash generator.
  std::string salted_key = salt + kDataReductionProxyKey + salt;
  base::string16 expected_hash = base::UTF8ToUTF16(base::MD5String(salted_key));
  EXPECT_EQ(expected_hash,
            DataReductionProxyRequestOptions::AuthHashForSalt(
                8675309, kDataReductionProxyKey));
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationOnIOThread) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession2, kExpectedCredentials2, kClientStr,
                        kExpectedBuild, kExpectedPatch, std::string(),
                        std::vector<std::string>(), &expected_header);

  std::string expected_header2;
  SetHeaderExpectations("86401-1633771873-1633771873-1633771873",
                        "d7c1c34ef6b90303b01c48a6c1db6419", kClientStr,
                        kExpectedBuild, kExpectedPatch, std::string(),
                        std::vector<std::string>(), &expected_header2);

  CreateRequestOptions(kVersion);
  test_context_->RunUntilIdle();

  // Now set a key.
  request_options()->SetKeyOnIO(kTestKey2);

  // Don't write headers if the proxy is invalid.
  VerifyExpectedHeader(std::string(), std::string());

  // Don't write headers with a valid proxy, that's not a data reduction proxy.
  VerifyExpectedHeader(kOtherProxy, std::string());

  // Don't write headers with a valid data reduction ssl proxy.
  VerifyExpectedHeader(params()->DefaultSSLOrigin(), std::string());

  // Write headers with a valid data reduction proxy.
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);

  // Write headers with a valid data reduction ssl proxy when one is expected.
  net::HttpRequestHeaders ssl_headers;
  request_options()->MaybeAddProxyTunnelRequestHandler(
      net::ProxyServer::FromURI(
        params()->DefaultSSLOrigin(),
        net::ProxyServer::SCHEME_HTTP).host_port_pair(),
      &ssl_headers);
  EXPECT_TRUE(ssl_headers.HasHeader(kChromeProxyHeader));
  std::string ssl_header_value;
  ssl_headers.GetHeader(kChromeProxyHeader, &ssl_header_value);
  EXPECT_EQ(expected_header, ssl_header_value);

  // Fast forward 24 hours. The header should be the same.
  request_options()->set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60));
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);

  // Fast forward one more second. The header should be new.
  request_options()->set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60 + 1));
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header2);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationIgnoresEmptyKey) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, kClientStr,
                        kExpectedBuild, kExpectedPatch, std::string(),
                        std::vector<std::string>(), &expected_header);
  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);

  // Now set an empty key. The auth handler should ignore that, and the key
  // remains |kTestKey|.
  request_options()->SetKeyOnIO(std::string());
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationBogusVersion) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession2, kExpectedCredentials2, kClientStr,
                        std::string(), std::string(), std::string(),
                        std::vector<std::string>(), &expected_header);

  CreateRequestOptions(kBogusVersion);

  // Now set a key.
  request_options()->SetKeyOnIO(kTestKey2);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationLoFi) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, kClientStr,
                        std::string(), std::string(), "low",
                        std::vector<std::string>(), &expected_header);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);

  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);
}

TEST_F(DataReductionProxyRequestOptionsTest, LoFiOn) {
  // Add the LoFi command line switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);

  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, kClientStr,
                        std::string(), std::string(), "low",
                        std::vector<std::string>(), &expected_header);

  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);
}

TEST_F(DataReductionProxyRequestOptionsTest, ParseExperiments) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyExperiment,
      "staging,\"foo,bar\"");
  std::vector<std::string> expected_experiments;
  expected_experiments.push_back("staging");
  expected_experiments.push_back("\"foo,bar\"");
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, kClientStr,
                        std::string(), std::string(), std::string(),
                        expected_experiments, &expected_header);

  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header);
}

}  // namespace data_reduction_proxy
