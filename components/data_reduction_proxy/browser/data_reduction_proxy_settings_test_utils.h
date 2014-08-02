// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_


#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "net/base/net_util.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;
class TestingPrefServiceSimple;

namespace data_reduction_proxy {

class TestDataReductionProxyConfig : public DataReductionProxyConfigurator {
 public:
  TestDataReductionProxyConfig();
  virtual ~TestDataReductionProxyConfig() {}
  virtual void Enable(bool restricted,
                      bool fallback_restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin,
                      const std::string& ssl_origin) OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void AddHostPatternToBypass(const std::string& pattern) OVERRIDE {}
  virtual void AddURLPatternToBypass(const std::string& pattern) OVERRIDE {}

  // True if the proxy has been enabled, i.e., only after |Enable| has been
  // called. Defaults to false.
  bool enabled_;

  // Describes whether the proxy has been put in a restricted mode. True if
  // |Enable| is called with |restricted| set to true. Defaults to false.
  bool restricted_;

  // Describes whether the proxy has been put in a mode where the fallback
  // configuration has been disallowed. True if |Enable| is called with
  // |fallback_restricted| set to true. Defaults to false.
  bool fallback_restricted_;

  // The origins that are passed to |Enable|.
  std::string origin_;
  std::string fallback_origin_;
  std::string ssl_origin_;
};

template <class C>
class MockDataReductionProxySettings : public C {
 public:
  MockDataReductionProxySettings<C>() : DataReductionProxySettings(
      new TestDataReductionProxyParams(
          DataReductionProxyParams::kAllowed |
          DataReductionProxyParams::kFallbackAllowed |
          DataReductionProxyParams::kPromoAllowed,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN)) {}
  MockDataReductionProxySettings<C>(int flags)
      : C(new TestDataReductionProxyParams(flags,
          TestDataReductionProxyParams::HAS_EVERYTHING &
          ~TestDataReductionProxyParams::HAS_DEV_ORIGIN)) {}
  MOCK_METHOD0(GetURLFetcherForAvailabilityCheck, net::URLFetcher*());
  MOCK_METHOD0(GetURLFetcherForWarmup, net::URLFetcher*());
  MOCK_METHOD0(GetOriginalProfilePrefs, PrefService*());
  MOCK_METHOD0(GetLocalStatePrefs, PrefService*());
  MOCK_METHOD3(LogProxyState, void(
      bool enabled, bool restricted, bool at_startup));
  MOCK_METHOD1(RecordProbeURLFetchResult,
               void(ProbeURLFetchResult result));
  MOCK_METHOD1(RecordStartupState,
               void(ProxyStartupState state));

  // SetProxyConfigs should always call LogProxyState exactly once.
  virtual void SetProxyConfigs(bool enabled,
                               bool alternative_enabled,
                               bool restricted,
                               bool at_startup) OVERRIDE {
    EXPECT_CALL(*this, LogProxyState(enabled, restricted, at_startup)).Times(1);
    C::SetProxyConfigs(enabled, alternative_enabled, restricted, at_startup);
  }
  virtual void GetNetworkList(net::NetworkInterfaceList* interfaces,
                              int policy) OVERRIDE {
    if (!network_interfaces_.get())
      return;
    for (size_t i = 0; i < network_interfaces_->size(); ++i)
      interfaces->push_back(network_interfaces_->at(i));
  }

  scoped_ptr<net::NetworkInterfaceList> network_interfaces_;
};

class DataReductionProxySettingsTestBase : public testing::Test {
 public:
  static void AddTestProxyToCommandLine();

  DataReductionProxySettingsTestBase();
  DataReductionProxySettingsTestBase(bool allowed,
                                     bool fallback_allowed,
                                     bool alt_allowed,
                                     bool promo_allowed);
  virtual ~DataReductionProxySettingsTestBase();

  void AddProxyToCommandLine();

  virtual void SetUp() OVERRIDE;

  template <class C> void ResetSettings(bool allowed,
                                        bool fallback_allowed,
                                        bool alt_allowed,
                                        bool promo_allowed,
                                        bool holdback);
  virtual void ResetSettings(bool allowed,
                             bool fallback_allowed,
                             bool alt_allowed,
                             bool promo_allowed,
                             bool holdback) = 0;

  template <class C> void SetProbeResult(
      const std::string& test_url,
      const std::string& warmup_test_url,
      const std::string& response,
      ProbeURLFetchResult state,
      bool success,
      int expected_calls);
  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& warmup_test_url,
                              const std::string& response,
                              ProbeURLFetchResult result,
                              bool success,
                              int expected_calls) = 0;

  void CheckProxyConfigs(bool expected_enabled,
                         bool expected_restricted,
                         bool expected_fallback_restricted);
  void CheckProbe(bool initially_enabled,
                  const std::string& probe_url,
                  const std::string& warmup_url,
                  const std::string& response,
                  bool request_success,
                  bool expected_enabled,
                  bool expected_restricted,
                  bool expected_fallback_restricted);
  void CheckProbeOnIPChange(const std::string& probe_url,
                            const std::string& warmup_url,
                            const std::string& response,
                            bool request_success,
                            bool expected_enabled,
                            bool expected_fallback_restricted);
  void CheckOnPrefChange(bool enabled, bool expected_enabled, bool managed);
  void CheckInitDataReductionProxy(bool enabled_at_startup);
  void RegisterSyntheticFieldTrialCallback(bool proxy_enabled) {
    proxy_enabled_ = proxy_enabled;
  }

  TestingPrefServiceSimple pref_service_;
  scoped_ptr<DataReductionProxySettings> settings_;
  scoped_ptr<TestDataReductionProxyParams> expected_params_;
  base::Time last_update_time_;
  bool proxy_enabled_;
};

// Test implementations should be subclasses of an instantiation of this
// class parameterized for whatever DataReductionProxySettings class
// is being tested.
template <class C>
class ConcreteDataReductionProxySettingsTest
    : public DataReductionProxySettingsTestBase {
 public:
  typedef MockDataReductionProxySettings<C> MockSettings;
  virtual void ResetSettings(bool allowed,
                             bool fallback_allowed,
                             bool alt_allowed,
                             bool promo_allowed,
                             bool holdback) OVERRIDE {
    return DataReductionProxySettingsTestBase::ResetSettings<C>(
        allowed, fallback_allowed, alt_allowed, promo_allowed, holdback);
  }

  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& warmup_test_url,
                              const std::string& response,
                              ProbeURLFetchResult result,
                              bool success,
                              int expected_calls) OVERRIDE {
    return DataReductionProxySettingsTestBase::SetProbeResult<C>(
        test_url,
        warmup_test_url,
        response,
        result,
        success,
        expected_calls);
  }
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
