// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_


#include "base/metrics/field_trial.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;
class TestingPrefServiceSimple;

template <class C>
class MockDataReductionProxySettings : public C {
 public:
  MOCK_METHOD0(GetURLFetcher, net::URLFetcher*());
  MOCK_METHOD0(GetOriginalProfilePrefs, PrefService*());
  MOCK_METHOD0(GetLocalStatePrefs, PrefService*());
  MOCK_METHOD2(LogProxyState, void(bool enabled, bool at_startup));

  // SetProxyConfigs should always call LogProxyState exactly once.
  virtual void SetProxyConfigs(bool enabled, bool at_startup) OVERRIDE {
    EXPECT_CALL(*this, LogProxyState(enabled, at_startup)).Times(1);
    C::SetProxyConfigs(enabled, at_startup);
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
      bool success,
      int expected_calls);
  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& response,
                              bool success,
                              int expected_calls) = 0;

  void CheckProxyPref(const std::string& expected_servers,
                      const std::string& expected_mode);
  void CheckProxyConfigs(bool expected_enabled);
  void CheckProbe(bool initially_enabled,
                  const std::string& probe_url,
                  const std::string& response,
                  bool request_success,
                  bool expected_enabled);
  void CheckProbeOnIPChange(const std::string& probe_url,
                            const std::string& response,
                            bool request_success,
                            bool expected_enabled);
  void CheckOnPrefChange(bool enabled,
                         const std::string& probe_url,
                         const std::string& response,
                         bool request_success,
                         bool expected_enabled);
  void CheckInitDataReductionProxy(bool enabled_at_startup);

  TestingPrefServiceSimple pref_service_;
  scoped_ptr<DataReductionProxySettings> settings_;
  base::Time last_update_time_;
  // This is a singleton that will clear all set field trials on destruction.
  scoped_ptr<base::FieldTrialList> field_trial_list_;
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
                              bool success,
                              int expected_calls) OVERRIDE {
    return DataReductionProxySettingsTestBase::SetProbeResult<C>(
  test_url, response, success, expected_calls);
  }
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_
