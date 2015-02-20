// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_statistics_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kProxy[] = "proxy";

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettingsTestBase::DataReductionProxySettingsTestBase()
    : testing::Test() {
}

DataReductionProxySettingsTestBase::~DataReductionProxySettingsTestBase() {}

// testing::Test implementation:
void DataReductionProxySettingsTestBase::SetUp() {
  test_context_.reset(new DataReductionProxyTestContext(
      DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN &
          ~TestDataReductionProxyParams::HAS_DEV_FALLBACK_ORIGIN,
      DataReductionProxyTestContext::USE_MOCK_CONFIG |
          DataReductionProxyTestContext::SKIP_SETTINGS_INITIALIZATION));

  PrefRegistrySimple* registry = test_context_->pref_service()->registry();
  registry->RegisterListPref(prefs::kDailyHttpOriginalContentLength);
  registry->RegisterListPref(prefs::kDailyHttpReceivedContentLength);
  registry->RegisterInt64Pref(prefs::kDailyHttpContentLengthLastUpdateDate,
                              0L);
  registry->RegisterDictionaryPref(kProxy);
  registry->RegisterBooleanPref(prefs::kDataReductionProxyEnabled, false);
  registry->RegisterBooleanPref(prefs::kDataReductionProxyAltEnabled, false);
  registry->RegisterBooleanPref(prefs::kDataReductionProxyWasEnabledBefore,
                                false);

  //AddProxyToCommandLine();
  ResetSettings(true, true, false, true, false);

  ListPrefUpdate original_update(test_context_->pref_service(),
                                 prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(test_context_->pref_service(),
                                 prefs::kDailyHttpReceivedContentLength);
  for (int64 i = 0; i < kNumDaysInHistory; i++) {
    original_update->Insert(0,
                            new base::StringValue(base::Int64ToString(2 * i)));
    received_update->Insert(0, new base::StringValue(base::Int64ToString(i)));
  }
  last_update_time_ = base::Time::Now().LocalMidnight();
  settings_->data_reduction_proxy_service()->statistics_prefs()->SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      last_update_time_.ToInternalValue());
}

template <class C>
void DataReductionProxySettingsTestBase::ResetSettings(bool allowed,
                                                       bool fallback_allowed,
                                                       bool alt_allowed,
                                                       bool promo_allowed,
                                                       bool holdback) {
  int flags = 0;
  if (allowed)
    flags |= DataReductionProxyParams::kAllowed;
  if (fallback_allowed)
    flags |= DataReductionProxyParams::kFallbackAllowed;
  if (alt_allowed)
    flags |= DataReductionProxyParams::kAlternativeAllowed;
  if (promo_allowed)
    flags |= DataReductionProxyParams::kPromoAllowed;
  if (holdback)
    flags |= DataReductionProxyParams::kHoldback;
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>();
  settings->config_ = test_context_->config();
  settings->data_reduction_proxy_service_ =
      test_context_->CreateDataReductionProxyService();
  test_context_->config()->ResetParamFlagsForTest(flags);
  settings->UpdateConfigValues();
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  settings_.reset(settings);
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::ResetSettings<DataReductionProxySettings>(
    bool allowed,
    bool fallback_allowed,
    bool alt_allowed,
    bool promo_allowed,
    bool holdback);

void DataReductionProxySettingsTestBase::ExpectSetProxyPrefs(
    bool expected_enabled,
    bool expected_alternate_enabled,
    bool expected_at_startup) {
  MockDataReductionProxyConfig* config =
      static_cast<MockDataReductionProxyConfig*>(test_context_->config());
  EXPECT_CALL(*config,
              SetProxyPrefs(expected_enabled, expected_alternate_enabled,
                            expected_at_startup));
}

void DataReductionProxySettingsTestBase::CheckOnPrefChange(
    bool enabled,
    bool expected_enabled,
    bool managed) {
  ExpectSetProxyPrefs(expected_enabled, false, false);
  if (managed) {
    test_context_->pref_service()->SetManagedPref(
        prefs::kDataReductionProxyEnabled, new base::FundamentalValue(enabled));
  } else {
    test_context_->pref_service()->SetBoolean(prefs::kDataReductionProxyEnabled,
                                              enabled);
  }
  test_context_->RunUntilIdle();
  // Never expect the proxy to be restricted for pref change tests.
}

void DataReductionProxySettingsTestBase::CheckInitDataReductionProxy(
    bool enabled_at_startup) {
  settings_->InitDataReductionProxySettings(
      test_context_->pref_service(), test_context_->io_data(),
      test_context_->CreateDataReductionProxyService());
  settings_->SetOnDataReductionEnabledCallback(
      base::Bind(&DataReductionProxySettingsTestBase::
                 RegisterSyntheticFieldTrialCallback,
                 base::Unretained(this)));

  test_context_->RunUntilIdle();
  EXPECT_EQ(enabled_at_startup, proxy_enabled_);
}

}  // namespace data_reduction_proxy
