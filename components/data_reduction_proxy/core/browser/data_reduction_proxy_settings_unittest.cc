// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include "base/command_line.h"
#include "base/md5.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
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

// TODO(marq): Add a test to verify that MaybeActivateDataReductionProxy
// is called when the pref in |settings_| is enabled.
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

  // TODO(bengr): Test enabling/disabling while a probe is outstanding.
  // The proxy is enabled and unrestructed initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, true, false);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, false, false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckMaybeActivateDataReductionProxy(true, false, true, true, false);
  // The proxy is disabled initially. Probes should not be emitted to change
  // state.
  CheckMaybeActivateDataReductionProxy(false, true, false, false, false);
}

TEST_F(DataReductionProxySettingsTest, TestOnProxyEnabledPrefChange) {
  settings_->InitPrefMembers();
  // The proxy is enabled initially.
  test_context_->config()->SetStateForTest(true, false, false, true);
  // The pref is disabled, so correspondingly should be the proxy.
  CheckOnPrefChange(false, false, false);
  // The pref is enabled, so correspondingly should be the proxy.
  CheckOnPrefChange(true, true, false);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            true);
  CheckInitDataReductionProxy(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));

  test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                            false);
  CheckInitDataReductionProxy(false);
}

TEST_F(DataReductionProxySettingsTest, TestEnableProxyFromCommandLine) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxy);
  CheckInitDataReductionProxy(true);
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
  settings_->SetOnDataReductionEnabledCallback(
      base::Bind(&DataReductionProxySettingsTestBase::
                 RegisterSyntheticFieldTrialCallback,
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
    test_context_->config()->params()->EnableQuic(enable_quic);

    settings_->SetOnDataReductionEnabledCallback(
        base::Bind(&DataReductionProxySettingsTestBase::
                   RegisterSyntheticFieldTrialCallback,
                   base::Unretained(this)));

    EXPECT_EQ(enable_quic,
              test_context_->config()->params()->origin().is_quic()) << i;
  }
}

}  // namespace data_reduction_proxy
