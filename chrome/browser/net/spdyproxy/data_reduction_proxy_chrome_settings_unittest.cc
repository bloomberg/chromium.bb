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
    test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .WithParamsFlags(
                data_reduction_proxy::DataReductionProxyParams::kAllowed |
                data_reduction_proxy::DataReductionProxyParams::
                    kFallbackAllowed |
                data_reduction_proxy::DataReductionProxyParams::kPromoAllowed)
            .WithParamsDefinitions(
                data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_EVERYTHING &
                ~data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_DEV_ORIGIN &
                ~data_reduction_proxy::TestDataReductionProxyParams::
                    HAS_DEV_FALLBACK_ORIGIN)
            .WithMockConfig()
            .SkipSettingsInitialization()
            .Build();
    config_ = test_context_->mock_config();
    drp_chrome_settings_->ResetConfigForTest(config_);
    dict_ = make_scoped_ptr(new base::DictionaryValue());

    PrefRegistrySimple* registry = test_context_->pref_service()->registry();
    registry->RegisterDictionaryPref(prefs::kProxy);
  }

  base::MessageLoopForIO message_loop_;
  scoped_ptr<DataReductionProxyChromeSettings> drp_chrome_settings_;
  scoped_ptr<base::DictionaryValue> dict_;
  scoped_ptr<data_reduction_proxy::DataReductionProxyTestContext> test_context_;
  data_reduction_proxy::MockDataReductionProxyConfig* config_;
};

TEST_F(DataReductionProxyChromeSettingsTest, MigrateEmptyProxy) {
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);
  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      test_context_->pref_service());

  EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateSystemProxy) {
  dict_->SetString("mode", "system");
  test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
  EXPECT_CALL(*config_, ContainsDataReductionProxy(_)).Times(0);

  drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
      test_context_->pref_service());

  EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateDataReductionProxy) {
  const std::string kTestServers[] = {"http=http://proxy.googlezip.net",
                                      "http=https://my-drp.org",
                                      "https=https://tunneldrp.com"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(true));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
  }
}

TEST_F(DataReductionProxyChromeSettingsTest,
       MigrateGooglezipDataReductionProxy) {
  const std::string kTestServers[] = {
      "http=http://proxy-dev.googlezip.net",
      "http=https://arbitraryprefix.googlezip.net",
      "https=https://tunnel.googlezip.net"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    // The proxy pref is set to a Data Reduction Proxy that doesn't match the
    // currently configured DRP, but the pref should still be cleared.
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(false));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    EXPECT_EQ(NULL, test_context_->pref_service()->GetUserPref(prefs::kProxy));
  }
}

TEST_F(DataReductionProxyChromeSettingsTest, MigrateIgnoreOtherProxy) {
  const std::string kTestServers[] = {
      "http=https://youtube.com",
      "http=http://googlezip.net",
      "http=http://thisismyproxynotgooglezip.net",
      "https=http://arbitraryprefixgooglezip.net"};

  for (const std::string& test_server : kTestServers) {
    dict_.reset(new base::DictionaryValue());
    dict_->SetString("mode", "fixed_servers");
    dict_->SetString("server", test_server);
    test_context_->pref_service()->Set(prefs::kProxy, *dict_.get());
    EXPECT_CALL(*config_, ContainsDataReductionProxy(_))
        .Times(1)
        .WillOnce(Return(false));

    drp_chrome_settings_->MigrateDataReductionProxyOffProxyPrefs(
        test_context_->pref_service());

    base::DictionaryValue* value =
        (base::DictionaryValue*)test_context_->pref_service()->GetUserPref(
            prefs::kProxy);
    std::string mode;
    EXPECT_TRUE(value->GetString("mode", &mode));
    EXPECT_EQ("fixed_servers", mode);
    std::string server;
    EXPECT_TRUE(value->GetString("server", &server));
    EXPECT_EQ(test_server, server);
  }
}
