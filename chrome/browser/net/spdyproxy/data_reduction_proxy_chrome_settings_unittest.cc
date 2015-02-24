// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

class DataReductionProxyChromeSettingsTest : public testing::Test {
 public:
  void SetUp() override {
    drp_chrome_settings_ =
        make_scoped_ptr(new DataReductionProxyChromeSettings());
    test_context_.reset(new data_reduction_proxy::DataReductionProxyTestContext(
        data_reduction_proxy::DataReductionProxyParams::kAllowed |
            data_reduction_proxy::DataReductionProxyParams::kFallbackAllowed |
            data_reduction_proxy::DataReductionProxyParams::kPromoAllowed,
        data_reduction_proxy::TestDataReductionProxyParams::HAS_EVERYTHING &
            ~data_reduction_proxy::TestDataReductionProxyParams::
                HAS_DEV_ORIGIN &
            ~data_reduction_proxy::TestDataReductionProxyParams::
                HAS_DEV_FALLBACK_ORIGIN,
        data_reduction_proxy::DataReductionProxyTestContext::USE_MOCK_CONFIG |
            data_reduction_proxy::DataReductionProxyTestContext::
                SKIP_SETTINGS_INITIALIZATION));
    config_ = test_context_->mock_config();
    drp_chrome_settings_->ResetConfigForTest(config_);
    dict_ = make_scoped_ptr(new base::DictionaryValue());
    mock_pref_service_ = make_scoped_ptr(new TestingPrefServiceSimple());

    PrefRegistrySimple* registry = mock_pref_service_->registry();
    registry->RegisterDictionaryPref(prefs::kProxy);
  }

  scoped_ptr<DataReductionProxyChromeSettings> drp_chrome_settings_;
  scoped_ptr<base::DictionaryValue> dict_;
  scoped_ptr<TestingPrefServiceSimple> mock_pref_service_;
  scoped_ptr<data_reduction_proxy::DataReductionProxyTestContext> test_context_;
  data_reduction_proxy::MockDataReductionProxyConfig* config_;
};

TEST_F(DataReductionProxyChromeSettingsTest, MigrateEmptyProxy) {
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);
  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateSystemProxy) {
  dict_->SetString("mode", "system");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateDataReductionProxy) {
  dict_->SetString("mode", "fixed_servers");
  dict_->SetString("server", "http=https://proxy.googlezip.net");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(1)
    .WillOnce(Return(true));

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  EXPECT_EQ(NULL, mock_pref_service_->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateIgnoreOtherProxy) {
  dict_->SetString("mode", "fixed_servers");
  dict_->SetString("server", "http=https://youtube.com");
  mock_pref_service_->Set(prefs::kProxy, *dict_.get());
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(1)
    .WillOnce(Return(false));

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      mock_pref_service_.get());

  base::DictionaryValue* value =
      (base::DictionaryValue*)mock_pref_service_->GetUserPref(prefs::kProxy);
  std::string mode;
  EXPECT_TRUE(value->GetString("mode", &mode));
  EXPECT_EQ("fixed_servers", mode);
  std::string server;
  EXPECT_TRUE(value->GetString("server", &server));
  EXPECT_EQ("http=https://youtube.com", server);
}
