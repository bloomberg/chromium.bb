// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/metrics/field_trial.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/socket/socket_test_util.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class BadEntropyProvider : public base::FieldTrial::EntropyProvider {
 public:
  ~BadEntropyProvider() override {}

  double GetEntropyForTrial(const std::string& trial_name,
                            uint32 randomization_seed) const override {
    return 0.5;
  }
};

class DataReductionProxySettingsTest
    : public ConcreteDataReductionProxySettingsTest<
          DataReductionProxySettings> {
 public:
  void CheckMaybeActivateDataReductionProxy(bool initially_enabled,
                                            bool request_succeeded,
                                            bool expected_enabled,
                                            bool expected_restricted,
                                            bool expected_fallback_restricted) {
    test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                              initially_enabled);
    test_context_->config()->SetStateForTest(initially_enabled, false,
                                             !request_succeeded, false);
    ExpectSetProxyPrefs(expected_enabled, false, false);
    settings_->MaybeActivateDataReductionProxy(false);
    test_context_->RunUntilIdle();
  }
};

TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  settings_->InitPrefMembers();
  // The proxy is disabled initially.
  test_context_->config()->SetStateForTest(false, false, false, false);

  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged());

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestCanUseDataReductionProxy) {
  settings_->InitPrefMembers();
  // The proxy is disabled initially.
  test_context_->config()->SetStateForTest(false, false, false, false);

  GURL http_gurl("http://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(http_gurl));

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->CanUseDataReductionProxy(http_gurl));

  GURL https_gurl("https://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(https_gurl));

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestResetDataReductionStatistics) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;
  settings_->ResetDataReductionStatistics();
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(0L, original_content_length);
  EXPECT_EQ(0L, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);
}

TEST_F(DataReductionProxySettingsTest, TestContentLengths) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_time;

  // Request |kNumDaysInHistory| days.
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  const unsigned int days = kNumDaysInHistory;
  // Received content length history values are 0 to |kNumDaysInHistory - 1|.
  int64 expected_total_received_content_length = (days - 1L) * days / 2;
  // Original content length history values are 0 to
  // |2 * (kNumDaysInHistory - 1)|.
  long expected_total_original_content_length = (days - 1L) * days;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);

  // Request |kNumDaysInHistory - 1| days.
  settings_->GetContentLengths(kNumDaysInHistory - 1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length -= (days - 1);
  expected_total_original_content_length -= 2 * (days - 1);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 0 days.
  settings_->GetContentLengths(0,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length = 0;
  expected_total_original_content_length = 0;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 1 day. First day had 0 bytes so should be same as 0 days.
  settings_->GetContentLengths(1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
}

TEST(DataReductionProxySettingsStandaloneTest, TestEndToEndSecureProxyCheck) {
  struct TestCase {
    const char* response_headers;
    const char* response_body;
    net::Error net_error_code;
    bool expected_restricted;
  };
  const TestCase kTestCases[] {
    { "HTTP/1.1 200 OK\r\n\r\n",
      "OK", net::OK, false,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "Bad", net::OK, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_FAILED, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_ABORTED, true,
    },
    // The secure proxy check shouldn't attempt to follow the redirect.
    { "HTTP/1.1 302 Found\r\nLocation: http://www.google.com/\r\n\r\n",
      "", net::OK, true,
    },
  };

  for (const TestCase& test_case : kTestCases) {
    net::TestURLRequestContext context(true);

    scoped_ptr<DataReductionProxyTestContext> drp_test_context =
        DataReductionProxyTestContext::Builder()
            .WithParamsFlags(DataReductionProxyParams::kAllowed |
                             DataReductionProxyParams::kFallbackAllowed |
                             DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                TestDataReductionProxyParams::HAS_EVERYTHING &
                    ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                    ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)
            .WithURLRequestContext(&context)
            .WithTestConfigurator()
            .SkipSettingsInitialization()
            .Build();

    context.set_net_log(drp_test_context->net_log());
    net::MockClientSocketFactory mock_socket_factory;
    context.set_client_socket_factory(&mock_socket_factory);
    context.Init();

    // Start with the Data Reduction Proxy disabled.
    drp_test_context->pref_service()->SetBoolean(
        prefs::kDataReductionProxyEnabled, false);
    drp_test_context->InitSettings();

    net::MockRead mock_reads[] = {
        net::MockRead(test_case.response_headers),
        net::MockRead(test_case.response_body),
        net::MockRead(net::SYNCHRONOUS, test_case.net_error_code),
    };
    net::StaticSocketDataProvider socket_data_provider(
        mock_reads, arraysize(mock_reads), nullptr, 0);
    mock_socket_factory.AddSocketDataProvider(&socket_data_provider);

    // Toggle the pref to trigger the secure proxy check.
    drp_test_context->pref_service()->SetBoolean(
            prefs::kDataReductionProxyEnabled, true);
    drp_test_context->RunUntilIdle();

    EXPECT_EQ(test_case.expected_restricted,
              drp_test_context->test_configurator()->restricted());
  }
}

TEST(DataReductionProxySettingsStandaloneTest, TestOnProxyEnabledPrefChange) {
  scoped_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithParamsFlags(DataReductionProxyParams::kAllowed |
                           DataReductionProxyParams::kFallbackAllowed |
                           DataReductionProxyParams::kPromoAllowed)
          .WithParamsDefinitions(
              TestDataReductionProxyParams::HAS_EVERYTHING &
                  ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
                  ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN)
          .WithMockConfig()
          .WithTestConfigurator()
          .WithMockDataReductionProxyService()
          .SkipSettingsInitialization()
          .Build();

  // The proxy is enabled initially.
  drp_test_context->config()->SetStateForTest(true, false, false, true);
  drp_test_context->InitSettings();

  // The pref is disabled, so correspondingly should be the proxy.
  EXPECT_CALL(*drp_test_context->mock_config(),
              SetProxyPrefs(false, false, false));
  drp_test_context->pref_service()->SetBoolean(
      prefs::kDataReductionProxyEnabled, false);

  // The pref is enabled, so correspondingly should be the proxy.
  EXPECT_CALL(*drp_test_context->mock_config(),
              SetProxyPrefs(true, false, false));
  drp_test_context->pref_service()->SetBoolean(
      prefs::kDataReductionProxyEnabled, true);
}

TEST_F(DataReductionProxySettingsTest, TestMaybeActivateDataReductionProxy) {
  // Initialize the pref member in |settings_| without the usual callback
  // so it won't trigger MaybeActivateDataReductionProxy when the pref value
  // is set.
  settings_->spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      settings_->GetOriginalProfilePrefs());
  settings_->data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a secure proxy check is
  // outstanding.
  // The proxy is enabled and unrestructed initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, true, false);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, false, false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckMaybeActivateDataReductionProxy(true, false, true, true, false);
  // The proxy is disabled initially. No secure proxy checks should take place,
  // and so the state should not change.
  CheckMaybeActivateDataReductionProxy(false, true, false, false, false);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            false);
  InitDataReductionProxy(false);
  CheckDataReductionProxySyntheticTrial(false);
}

TEST_F(DataReductionProxySettingsTest, TestEnableProxyFromCommandLine) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxy);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestSetDataReductionProxyEnabled) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));
  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxyLoFi);
  InitDataReductionProxy(true);

  ExpectSetProxyPrefs(false, false, false);
  settings_->SetDataReductionProxyEnabled(false);
  test_context_->RunUntilIdle();
  CheckDataReductionProxySyntheticTrial(false);
  CheckDataReductionProxyLoFiSyntheticTrial(false);

  ExpectSetProxyPrefs(true, false, false);
  settings->SetDataReductionProxyEnabled(true);
  CheckDataReductionProxySyntheticTrial(true);
  CheckDataReductionProxyLoFiSyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestEnableLoFiFromCommandLineProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxyLoFi);
  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  InitDataReductionProxy(true);
  CheckDataReductionProxyLoFiSyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestEnableLoFiFromCommandLineProxyOff) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxyLoFi);
  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            false);
  InitDataReductionProxy(false);
  CheckDataReductionProxyLoFiSyntheticTrial(false);
}

TEST_F(DataReductionProxySettingsTest, TestGetDailyContentLengths) {
  DataReductionProxySettings::ContentLengthList result =
      settings_->GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(kNumDaysInHistory, result.size());

  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    long expected_length =
        static_cast<long>((kNumDaysInHistory - 1 - i) * 2);
    ASSERT_EQ(expected_length, result[i]);
  }
}

TEST_F(DataReductionProxySettingsTest, CheckInitMetricsWhenNotAllowed) {
  // No call to |AddProxyToCommandLine()| was made, so the proxy feature
  // should be unavailable.
  // Clear the command line. Setting flags can force the proxy to be allowed.
  base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

  ResetSettings(false, false, false, false, false);
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_FALSE(settings->allowed_);
  EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

  settings_->InitDataReductionProxySettings(
      test_context_->pref_service(), test_context_->io_data(),
      test_context_->CreateDataReductionProxyService());
  settings_->SetCallbackToRegisterSyntheticFieldTrial(
      base::Bind(&DataReductionProxySettingsTestBase::
                 SyntheticFieldTrialRegistrationCallback,
                 base::Unretained(this)));

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, CheckQUICFieldTrials) {
  for (int i = 0; i < 2; ++i) {
    bool enable_quic = i == 0;
    // No call to |AddProxyToCommandLine()| was made, so the proxy feature
    // should be unavailable.
    // Clear the command line. Setting flags can force the proxy to be allowed.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, NULL);

    ResetSettings(false, false, false, false, false);
    MockSettings* settings = static_cast<MockSettings*>(settings_.get());
    EXPECT_FALSE(settings->Allowed());
    EXPECT_CALL(*settings, RecordStartupState(PROXY_NOT_AVAILABLE));

    settings_->InitDataReductionProxySettings(
         test_context_->pref_service(), test_context_->io_data(),
         test_context_->CreateDataReductionProxyService());

    base::FieldTrialList field_trial_list(new BadEntropyProvider());
    if (enable_quic) {
      base::FieldTrialList::CreateFieldTrial(
          DataReductionProxyParams::GetQuicFieldTrialName(),
          "Enabled");
    } else {
      base::FieldTrialList::CreateFieldTrial(
          DataReductionProxyParams::GetQuicFieldTrialName(),
          "Disabled");
    }
    test_context_->config()->EnableQuic(enable_quic);

    settings_->SetCallbackToRegisterSyntheticFieldTrial(
        base::Bind(&DataReductionProxySettingsTestBase::
                   SyntheticFieldTrialRegistrationCallback,
                   base::Unretained(this)));

    EXPECT_EQ(enable_quic,
              test_context_->config()->Origin().is_quic()) << i;
  }
}

}  // namespace data_reduction_proxy
