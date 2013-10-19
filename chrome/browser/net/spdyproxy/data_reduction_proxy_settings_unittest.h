// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_


#include "base/metrics/field_trial.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;
class TestingPrefServiceSimple;

class DataReductionProxySettingsTestBase : public testing::Test {
 public:
  DataReductionProxySettingsTestBase();
  virtual ~DataReductionProxySettingsTestBase();

  void AddProxyToCommandLine();
  virtual void ResetSettings() = 0;
  virtual DataReductionProxySettings* Settings() = 0;
  virtual void SetProbeResult(const std::string& test_url,
                              const std::string& response,
                              bool success) = 0;

  virtual void SetUp() OVERRIDE;
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
  base::Time last_update_time_;
  // This is a singleton that will clear all set field trials on destruction.
  scoped_ptr<base::FieldTrialList> field_trial_list_;
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_UNITTEST_H_
