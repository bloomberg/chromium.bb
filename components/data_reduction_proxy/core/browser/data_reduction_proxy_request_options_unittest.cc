// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kChromeProxyHeader[] = "chrome-proxy";
const char kOtherProxy[] = "testproxy:17";

const char kVersion[] = "0.1.2.3";
const char kExpectedBuild[] = "2";
const char kExpectedPatch[] = "3";
const char kBogusVersion[] = "0.0";
const char kExpectedCredentials[] = "96bd72ec4a050ba60981743d41787768";
const char kExpectedSession[] = "0-1633771873-1633771873-1633771873";

const char kTestKey2[] = "test-key2";
const char kExpectedCredentials2[] = "c911fdb402f578787562cf7f00eda972";
const char kExpectedSession2[] = "0-1633771873-1633771873-1633771873";
const char kDataReductionProxyKey[] = "12345";

const char kSecureSession[] = "TestSecureSessionKey";
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

void SetHeaderExpectations(const std::string& session,
                           const std::string& credentials,
                           const std::string& secure_session,
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
  if (!secure_session.empty()) {
    expected_options.push_back(std::string(kSecureSessionHeaderOption) + "=" +
                               secure_session);
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
    *expected_header = base::JoinString(expected_options, ", ");
}

}  // namespace

class DataReductionProxyRequestOptionsTest : public testing::Test {
 public:
  DataReductionProxyRequestOptionsTest() {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(
                 DataReductionProxyParams::kAllowAllProxyConfigurations)
            .WithParamsDefinitions(
                 TestDataReductionProxyParams::HAS_EVERYTHING &
                 ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                 ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)
            .Build();
  }

  void CreateRequestOptions(const std::string& version) {
    request_options_.reset(new TestDataReductionProxyRequestOptions(
        kClient, version, test_context_->config()));
    request_options_->Init();
  }

  void CreateRequest() {
    net::URLRequestContext* context =
        test_context_->request_context_getter()->GetURLRequestContext();
    request_ = context->CreateRequest(GURL(), net::DEFAULT_PRIORITY, nullptr);
  }

  const net::URLRequest& request() { return *request_.get(); }

  TestDataReductionProxyParams* params() {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyRequestOptions* request_options() {
    return request_options_.get();
  }

  void VerifyExpectedHeader(const std::string& proxy_uri,
                            const std::string& expected_header,
                            bool should_request_lofi_resource) {
    test_context_->RunUntilIdle();
    net::HttpRequestHeaders headers;
    request_options_->MaybeAddRequestHeader(
        request_.get(),
        proxy_uri.empty() ? net::ProxyServer()
                          : net::ProxyServer::FromURI(
                                proxy_uri, net::ProxyServer::SCHEME_HTTP),
        &headers, should_request_lofi_resource);
    if (expected_header.empty()) {
      EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));
      return;
    }
    EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
    std::string header_value;
    headers.GetHeader(kChromeProxyHeader, &header_value);
    EXPECT_EQ(expected_header, header_value);
  }

  base::MessageLoopForIO message_loop_;
  scoped_ptr<TestDataReductionProxyRequestOptions> request_options_;
  scoped_ptr<DataReductionProxyTestContext> test_context_;
  scoped_ptr<net::URLRequest> request_;
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
  SetHeaderExpectations(kExpectedSession2, kExpectedCredentials2, std::string(),
                        kClientStr, kExpectedBuild, kExpectedPatch,
                        std::string(), std::vector<std::string>(),
                        &expected_header);

  std::string expected_header2;
  SetHeaderExpectations("86401-1633771873-1633771873-1633771873",
                        "d7c1c34ef6b90303b01c48a6c1db6419", std::string(),
                        kClientStr, kExpectedBuild, kExpectedPatch,
                        std::string(), std::vector<std::string>(),
                        &expected_header2);

  CreateRequestOptions(kVersion);
  test_context_->RunUntilIdle();

  // Now set a key.
  request_options()->SetKeyOnIO(kTestKey2);

  // Don't write headers if the proxy is invalid.
  VerifyExpectedHeader(std::string(), std::string(), false);

  // Don't write headers with a valid proxy, that's not a data reduction proxy.
  VerifyExpectedHeader(kOtherProxy, std::string(), false);

  // Don't write headers with a valid data reduction ssl proxy.
  VerifyExpectedHeader(params()->DefaultSSLOrigin(), std::string(), false);

  // Write headers with a valid data reduction proxy.
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);

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
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);

  // Fast forward one more second. The header should be new.
  request_options()->set_offset(base::TimeDelta::FromSeconds(24 * 60 * 60 + 1));
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header2, false);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationIgnoresEmptyKey) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, std::string(),
                        kClientStr, kExpectedBuild, kExpectedPatch,
                        std::string(), std::vector<std::string>(),
                        &expected_header);
  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);

  // Now set an empty key. The auth handler should ignore that, and the key
  // remains |kTestKey|.
  request_options()->SetKeyOnIO(std::string());
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationBogusVersion) {
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession2, kExpectedCredentials2, std::string(),
                        kClientStr, std::string(), std::string(), std::string(),
                        std::vector<std::string>(), &expected_header);

  CreateRequestOptions(kBogusVersion);

  // Now set a key.
  request_options()->SetKeyOnIO(kTestKey2);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);
}

TEST_F(DataReductionProxyRequestOptionsTest, LoFiOnThroughCommandLineSwitch) {
  test_context_->config()->ResetLoFiStatusForTest();
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, std::string(),
                        kClientStr, std::string(), std::string(), std::string(),
                        std::vector<std::string>(), &expected_header);
  CreateRequest();
  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(
      params()->DefaultOrigin(), expected_header,
      test_context_->config()->ShouldEnableLoFiMode(request()));

  test_context_->config()->ResetLoFiStatusForTest();
  // Add the LoFi command line switch.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyLoFi,
      switches::kDataReductionProxyLoFiValueAlwaysOn);

  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, std::string(),
                        kClientStr, std::string(), std::string(), "low",
                        std::vector<std::string>(), &expected_header);

  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(
      params()->DefaultOrigin(), expected_header,
      test_context_->config()->ShouldEnableLoFiMode(request()));
}

TEST_F(DataReductionProxyRequestOptionsTest, AutoLoFi) {
  const struct {
    bool auto_lofi_enabled_group;
    bool auto_lofi_control_group;
    bool network_prohibitively_slow;
  } tests[] = {
      {false, false, false},
      {false, false, true},
      {true, false, false},
      {true, false, true},
      {false, true, false},
      {false, true, true},
      // Repeat this test data to simulate user moving out of Lo-Fi control
      // experiment.
      {false, true, false},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    test_context_->config()->ResetLoFiStatusForTest();
    // Lo-Fi header is expected only if session is part of Lo-Fi enabled field
    // trial and network is prohibitively slow.
    bool expect_lofi_header =
        tests[i].auto_lofi_enabled_group && tests[i].network_prohibitively_slow;
    bool expect_lofi_experiment_header =
        tests[i].auto_lofi_control_group && tests[i].network_prohibitively_slow;

    std::string expected_header;
    if (!expect_lofi_header) {
      SetHeaderExpectations(kExpectedSession, kExpectedCredentials,
                            std::string(), kClientStr, std::string(),
                            std::string(), std::string(),
                            std::vector<std::string>(), &expected_header);
    } else {
      SetHeaderExpectations(kExpectedSession, kExpectedCredentials,
                            std::string(), kClientStr, std::string(),
                            std::string(), "low", std::vector<std::string>(),
                            &expected_header);
    }

    if (expect_lofi_experiment_header) {
      expected_header = expected_header.append(", exp=");
      expected_header = expected_header.append(kLoFiExperimentID);
    }

    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    if (tests[i].auto_lofi_control_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Control");
    }

    test_context_->config()->SetNetworkProhibitivelySlow(
        tests[i].network_prohibitively_slow);

    CreateRequest();

    CreateRequestOptions(kBogusVersion);
    VerifyExpectedHeader(
        params()->DefaultOrigin(), expected_header,
        test_context_->config()->ShouldEnableLoFiMode(request()));
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, SlowConnectionsFlag) {
  const struct {
    bool slow_connections_flag_enabled;
    bool network_prohibitively_slow;
    bool auto_lofi_enabled_group;

  } tests[] = {
      {
          false, false, false,
      },
      {
          false, true, false,
      },
      {
          true, false, false,
      },
      {
          true, true, false,
      },
      {
          false, false, true,
      },
      {
          false, true, true,
      },
      {
          true, false, true,
      },
      {
          true, true, true,
      },
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    test_context_->config()->ResetLoFiStatusForTest();
    // For the purpose of this test, Lo-Fi header is expected only if LoFi Slow
    // Connection Flag is enabled or session is part of Lo-Fi enabled field
    // trial. For both cases, an additional condition is that network must be
    // prohibitively slow.
    bool expect_lofi_header = (tests[i].slow_connections_flag_enabled &&
                               tests[i].network_prohibitively_slow) ||
                              (!tests[i].slow_connections_flag_enabled &&
                               tests[i].auto_lofi_enabled_group &&
                               tests[i].network_prohibitively_slow);

    std::string expected_header;
    if (!expect_lofi_header) {
      SetHeaderExpectations(kExpectedSession, kExpectedCredentials,
                            std::string(), kClientStr, std::string(),
                            std::string(), std::string(),
                            std::vector<std::string>(), &expected_header);
    } else {
      SetHeaderExpectations(kExpectedSession, kExpectedCredentials,
                            std::string(), kClientStr, std::string(),
                            std::string(), "low", std::vector<std::string>(),
                            &expected_header);
    }

    if (tests[i].slow_connections_flag_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyLoFi,
          switches::kDataReductionProxyLoFiValueSlowConnectionsOnly);
    }

    base::FieldTrialList field_trial_list(nullptr);
    if (tests[i].auto_lofi_enabled_group) {
      base::FieldTrialList::CreateFieldTrial(params::GetLoFiFieldTrialName(),
                                             "Enabled");
    }

    test_context_->config()->SetNetworkProhibitivelySlow(
        tests[i].network_prohibitively_slow);

    CreateRequest();

    CreateRequestOptions(kBogusVersion);
    VerifyExpectedHeader(
        params()->DefaultOrigin(), expected_header,
        test_context_->config()->ShouldEnableLoFiMode(request()));
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, SecureSession) {
  std::string expected_header;
  SetHeaderExpectations(std::string(), std::string(), kSecureSession,
                        kClientStr, std::string(), std::string(), std::string(),
                        std::vector<std::string>(), &expected_header);

  CreateRequestOptions(kBogusVersion);
  request_options()->SetSecureSession(kSecureSession);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);
}

TEST_F(DataReductionProxyRequestOptionsTest, ParseExperiments) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyExperiment,
      "staging,\"foo,bar\"");
  std::vector<std::string> expected_experiments;
  expected_experiments.push_back("staging");
  expected_experiments.push_back("\"foo,bar\"");
  std::string expected_header;
  SetHeaderExpectations(kExpectedSession, kExpectedCredentials, std::string(),
                        kClientStr, std::string(), std::string(), std::string(),
                        expected_experiments, &expected_header);

  CreateRequestOptions(kBogusVersion);
  VerifyExpectedHeader(params()->DefaultOrigin(), expected_header, false);
}

TEST_F(DataReductionProxyRequestOptionsTest, ParseLocalSessionKey) {
  const struct {
    bool should_succeed;
    std::string session_key;
    std::string expected_session;
    std::string expected_credentials;
  } tests[] = {
      {
          true,
          "foobar|1234",
          "foobar",
          "1234",
      },
      {
          false,
          "foobar|1234|foobaz",
          std::string(),
          std::string(),
      },
      {
          false,
          "foobar",
          std::string(),
          std::string(),
      },
      {
          false,
          std::string(),
          std::string(),
          std::string(),
      },
  };

  std::string session;
  std::string credentials;
  for (size_t i = 0; i < arraysize(tests); ++i) {
    EXPECT_EQ(tests[i].should_succeed,
              DataReductionProxyRequestOptions::ParseLocalSessionKey(
                  tests[i].session_key, &session, &credentials));
    if (tests[i].should_succeed) {
      EXPECT_EQ(tests[i].expected_session, session);
      EXPECT_EQ(tests[i].expected_credentials, credentials);
    }
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, PopulateConfigResponse) {
  CreateRequestOptions(kBogusVersion);
  ClientConfig config;
  request_options()->PopulateConfigResponse(&config);
  EXPECT_EQ(
      "0-1633771873-1633771873-1633771873|96bd72ec4a050ba60981743d41787768",
      config.session_key());
  EXPECT_EQ(86400, config.refresh_time().seconds());
  EXPECT_EQ(0, config.refresh_time().nanos());
}

}  // namespace data_reduction_proxy
