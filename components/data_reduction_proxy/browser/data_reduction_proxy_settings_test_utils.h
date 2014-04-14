// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_


#include "base/prefs/testing_pref_service.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;
class TestingPrefServiceSimple;

namespace data_reduction_proxy {

class TestDataReductionProxyConfig : public DataReductionProxyConfigurator {
 public:
  TestDataReductionProxyConfig()
      : enabled_(false), restricted_(false) {}
  virtual ~TestDataReductionProxyConfig() {}
  virtual void Enable(bool restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin) OVERRIDE;
  virtual void Disable() OVERRIDE;
  virtual void AddHostPatternToBypass(const std::string& pattern) OVERRIDE {}
  virtual void AddURLPatternToBypass(const std::string& pattern) OVERRIDE {}

  // True if the proxy has been enabled, i.e., only after |Enable| has been
  // called. Defaults to false.
  bool enabled_;

  // Describes whether the proxy has been put in a restricted mode. True if
  // |Enable| is called with |restricted| set to true. Defaults to false.
  bool restricted_;
};

template <class C>
class MockDataReductionProxySettings : public C {
 public:
  MOCK_METHOD0(GetURLFetcher, net::URLFetcher*());
  MOCK_METHOD0(GetOriginalProfilePrefs, PrefService*());
  MOCK_METHOD0(GetLocalStatePrefs, PrefService*());
  MOCK_METHOD3(LogProxyState, void(
      bool enabled, bool restricted, bool at_startup));
  MOCK_METHOD1(RecordProbeURLFetchResult,
               void(ProbeURLFetchResult result));
  MOCK_METHOD1(RecordStartupState,
               void(ProxyStartupState state));

  // SetProxyConfigs should always call LogProxyState exactly once.
  virtual void SetProxyConfigs(
      bool enabled, bool restricted, bool at_startup) OVERRIDE {
    EXPECT_CALL(*this, LogProxyState(enabled, restricted, at_startup)).Times(1);
    C::SetProxyConfigs(enabled, restricted, at_startup);
  }
};

class DataReductionProxySettingsTestBase : public testing::Test {
 public:
  DataReductionProxySettingsTestBase();
  virtual ~DataReductionProxySettingsTestBase();

  void AddProxyToCommandLine();

  virtual void SetUp() OVERRIDE;

  template <class C> void ResetSettings();
  virtual void ResetSettings() = 0;

  template <class C> void SetProbeResult(
      const std::string& test_url,
      const std::string& response,
      ProbeURLFetchResult state,
      bool success,
      int expected_calls);
  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& response,
                              ProbeURLFetchResult result,
                              bool success,
                              int expected_calls) = 0;

  void CheckProxyConfigs(bool expected_enabled, bool expected_restricted);
  void CheckProbe(bool initially_enabled,
                  const std::string& probe_url,
                  const std::string& response,
                  bool request_success,
                  bool expected_enabled,
                  bool expected_restricted);
  void CheckProbeOnIPChange(const std::string& probe_url,
                            const std::string& response,
                            bool request_success,
                            bool expected_enabled);
  void CheckOnPrefChange(bool enabled, bool expected_enabled);
  void CheckInitDataReductionProxy(bool enabled_at_startup);

  TestingPrefServiceSimple pref_service_;
  scoped_ptr<DataReductionProxySettings> settings_;
  base::Time last_update_time_;
};

// Test implementations should be subclasses of an instantiation of this
// class parameterized for whatever DataReductionProxySettings class
// is being tested.
template <class C>
class ConcreteDataReductionProxySettingsTest
    : public DataReductionProxySettingsTestBase {
 public:
  typedef MockDataReductionProxySettings<C> MockSettings;
  virtual void ResetSettings() OVERRIDE {
    return DataReductionProxySettingsTestBase::ResetSettings<C>();
  }

  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& response,
                              ProbeURLFetchResult result,
                              bool success,
                              int expected_calls) OVERRIDE {
    return DataReductionProxySettingsTestBase::SetProbeResult<C>(
  test_url, response, result, success, expected_calls);
  }
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_BROWSER_DATA_REDUCTION_PROXY_SETTINGS_TEST_UTILS_H_
