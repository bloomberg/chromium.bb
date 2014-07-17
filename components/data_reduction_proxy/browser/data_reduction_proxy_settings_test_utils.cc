// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings_test_utils.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace {

const char kProbeURLWithOKResponse[] = "http://ok.org/";
const char kWarmupURLWithNoContentResponse[] = "http://warm.org/";

const char kProxy[] = "proxy";

}  // namespace

namespace data_reduction_proxy {

// Transform "normal"-looking headers (\n-separated) to the appropriate
// input format for ParseRawHeaders (\0-separated).
void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    *headers += '\0';
}

ProbeURLFetchResult FetchResult(bool enabled, bool success) {
  if (enabled) {
    if (success)
      return SUCCEEDED_PROXY_ALREADY_ENABLED;
    return FAILED_PROXY_DISABLED;
  }
  if (success)
    return SUCCEEDED_PROXY_ENABLED;
  return FAILED_PROXY_ALREADY_DISABLED;
}

TestDataReductionProxyConfig::TestDataReductionProxyConfig()
    : enabled_(false),
      restricted_(false),
      fallback_restricted_(false) {}

void TestDataReductionProxyConfig::Enable(
    bool restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  enabled_ = true;
  restricted_ = restricted;
  fallback_restricted_ = fallback_restricted;
  origin_ = primary_origin;
  fallback_origin_ = fallback_origin;
  ssl_origin_ = ssl_origin;
}

void TestDataReductionProxyConfig::Disable() {
  enabled_ = false;
  restricted_ = false;
  fallback_restricted_ = false;
  origin_ = "";
  fallback_origin_ = "";
  ssl_origin_ = "";
}

DataReductionProxySettingsTestBase::DataReductionProxySettingsTestBase()
    : testing::Test() {
}

DataReductionProxySettingsTestBase::~DataReductionProxySettingsTestBase() {}

// testing::Test implementation:
void DataReductionProxySettingsTestBase::SetUp() {
  PrefRegistrySimple* registry = pref_service_.registry();
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

  ListPrefUpdate original_update(&pref_service_,
                                 prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(&pref_service_,
                                 prefs::kDailyHttpReceivedContentLength);
  for (int64 i = 0; i < kNumDaysInHistory; i++) {
    original_update->Insert(0,
                            new base::StringValue(base::Int64ToString(2 * i)));
    received_update->Insert(0, new base::StringValue(base::Int64ToString(i)));
  }
  last_update_time_ = base::Time::Now().LocalMidnight();
  pref_service_.SetInt64(
      prefs::kDailyHttpContentLengthLastUpdateDate,
      last_update_time_.ToInternalValue());
  expected_params_.reset(new TestDataReductionProxyParams(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN));
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
      new MockDataReductionProxySettings<C>(flags);
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(&pref_service_));
  EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck()).Times(0);
  EXPECT_CALL(*settings, GetURLFetcherForWarmup()).Times(0);
  EXPECT_CALL(*settings, LogProxyState(_, _, _)).Times(0);
  settings_.reset(settings);
  settings_->configurator_.reset(new TestDataReductionProxyConfig());
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::ResetSettings<DataReductionProxySettings>(
    bool allowed,
    bool fallback_allowed,
    bool alt_allowed,
    bool promo_allowed,
    bool holdback);

template <class C>
void DataReductionProxySettingsTestBase::SetProbeResult(
    const std::string& test_url,
    const std::string& warmup_test_url,
    const std::string& response,
    ProbeURLFetchResult result,
    bool success,
    int expected_calls)  {
  MockDataReductionProxySettings<C>* settings =
      static_cast<MockDataReductionProxySettings<C>*>(settings_.get());
  if (0 == expected_calls) {
    EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck()).Times(0);
    EXPECT_CALL(*settings, GetURLFetcherForWarmup()).Times(0);
    EXPECT_CALL(*settings, RecordProbeURLFetchResult(_)).Times(0);
  } else {
    EXPECT_CALL(*settings, RecordProbeURLFetchResult(result)).Times(1);
    EXPECT_CALL(*settings, GetURLFetcherForAvailabilityCheck())
        .Times(expected_calls)
        .WillRepeatedly(Return(new net::FakeURLFetcher(
            GURL(test_url),
            settings,
            response,
            success ? net::HTTP_OK : net::HTTP_INTERNAL_SERVER_ERROR,
            success ? net::URLRequestStatus::SUCCESS :
                      net::URLRequestStatus::FAILED)));
    EXPECT_CALL(*settings, GetURLFetcherForWarmup())
        .Times(expected_calls)
        .WillRepeatedly(Return(new net::FakeURLFetcher(
            GURL(warmup_test_url),
            settings,
            "",
            success ? net::HTTP_NO_CONTENT : net::HTTP_INTERNAL_SERVER_ERROR,
                success ? net::URLRequestStatus::SUCCESS :
                          net::URLRequestStatus::FAILED)));
  }
}

// Explicitly generate required instantiations.
template void
DataReductionProxySettingsTestBase::SetProbeResult<DataReductionProxySettings>(
    const std::string& test_url,
    const std::string& warmup_test_url,
    const std::string& response,
    ProbeURLFetchResult result,
    bool success,
    int expected_calls);

void DataReductionProxySettingsTestBase::CheckProxyConfigs(
    bool expected_enabled,
    bool expected_restricted,
    bool expected_fallback_restricted) {
  TestDataReductionProxyConfig* config =
      static_cast<TestDataReductionProxyConfig*>(
          settings_->configurator_.get());
  ASSERT_EQ(expected_restricted, config->restricted_);
  ASSERT_EQ(expected_fallback_restricted, config->fallback_restricted_);
  ASSERT_EQ(expected_enabled, config->enabled_);
}

void DataReductionProxySettingsTestBase::CheckProbe(
    bool initially_enabled,
    const std::string& probe_url,
    const std::string& warmup_url,
    const std::string& response,
    bool request_succeeded,
    bool expected_enabled,
    bool expected_restricted,
    bool expected_fallback_restricted) {
  pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled,
                           initially_enabled);
  if (initially_enabled)
    settings_->enabled_by_user_ = true;
  settings_->restricted_by_carrier_ = false;
  SetProbeResult(probe_url,
                 warmup_url,
                 response,
                 FetchResult(initially_enabled,
                             request_succeeded && (response == "OK")),
                 request_succeeded,
                 initially_enabled ? 1 : 0);
  settings_->MaybeActivateDataReductionProxy(false);
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(expected_enabled,
                    expected_restricted,
                    expected_fallback_restricted);
}

void DataReductionProxySettingsTestBase::CheckProbeOnIPChange(
    const std::string& probe_url,
    const std::string& warmup_url,
    const std::string& response,
    bool request_succeeded,
    bool expected_restricted,
    bool expected_fallback_restricted) {
  SetProbeResult(probe_url,
                 warmup_url,
                 response,
                 FetchResult(!settings_->restricted_by_carrier_,
                             request_succeeded && (response == "OK")),
                 request_succeeded,
                 1);
  settings_->OnIPAddressChanged();
  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(true, expected_restricted, expected_fallback_restricted);
}

void DataReductionProxySettingsTestBase::CheckOnPrefChange(
    bool enabled,
    bool expected_enabled,
    bool managed) {
  // Always have a sucessful probe for pref change tests.
  SetProbeResult(kProbeURLWithOKResponse,
                 kWarmupURLWithNoContentResponse,
                 "OK",
                 FetchResult(enabled, true),
                 true,
                 expected_enabled ? 1 : 0);
  if (managed) {
    pref_service_.SetManagedPref(prefs::kDataReductionProxyEnabled,
                                 new base::FundamentalValue(enabled));
  } else {
    pref_service_.SetBoolean(prefs::kDataReductionProxyEnabled, enabled);
  }
  base::MessageLoop::current()->RunUntilIdle();
  // Never expect the proxy to be restricted for pref change tests.
  CheckProxyConfigs(expected_enabled, false, false);
}

void DataReductionProxySettingsTestBase::CheckInitDataReductionProxy(
    bool enabled_at_startup) {
  base::MessageLoopForUI loop;
  SetProbeResult(kProbeURLWithOKResponse,
                 kWarmupURLWithNoContentResponse,
                 "OK",
                 FetchResult(enabled_at_startup, true),
                 true,
                 enabled_at_startup ? 1 : 0);
  scoped_ptr<DataReductionProxyConfigurator> configurator(
      new TestDataReductionProxyConfig());
  settings_->SetProxyConfigurator(configurator.Pass());
  scoped_refptr<net::TestURLRequestContextGetter> request_context =
      new net::TestURLRequestContextGetter(base::MessageLoopProxy::current());
  settings_->InitDataReductionProxySettings(&pref_service_,
                                            &pref_service_,
                                            request_context.get());

  base::MessageLoop::current()->RunUntilIdle();
  CheckProxyConfigs(enabled_at_startup, false, false);
}

}  // namespace data_reduction_proxy
