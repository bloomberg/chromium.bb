// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using data_reduction_proxy::DataReductionProxyParams;
using data_reduction_proxy::DataReductionProxySettings;

class DataReductionProxyChromeSettingsTest : public testing::Test {
 public:
  void SetUp() override {
    drp_chrome_settings_ =
        make_scoped_ptr(new DataReductionProxyChromeSettings());
    dict_ = make_scoped_ptr(new DictionaryValue());
    mock_pref_service_ = make_scoped_ptr(new TestingPrefServiceSimple());

    PrefRegistrySimple* registry = mock_pref_service_->registry();
    registry->RegisterDictionaryPref(prefs::kProxy);
  }

  scoped_ptr<DataReductionProxyChromeSettings> drp_chrome_settings_;
  scoped_ptr<base::DictionaryValue> dict_;
  scoped_ptr<TestingPrefServiceSimple> mock_pref_service_;
};

TEST_F(DataReductionProxyChromeSettingsTest, MigrateEmptyProxy) {
  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateSystemProxy) {
  dict_->SetString("mode", "system");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateDataReductionProxy) {
  dict_->SetString("mode", "fixed_servers");
  dict_->SetString("server", "http=https://proxy.googlezip.net");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateIgnoreOtherProxy) {
  dict_->SetString("mode", "fixed_servers");
  dict_->SetString("server", "http=https://youtube.com");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  DictionaryValue* value =
      (DictionaryValue*)mock_pref_service_->GetUserPref(prefs::kProxy);
  std::string mode;
  EXPECT_TRUE(value->GetString("mode", &mode));
  EXPECT_EQ("fixed_servers", mode);
  std::string server;
  EXPECT_TRUE(value->GetString("server", &server));
  EXPECT_EQ("http=https://youtube.com", server);
}
