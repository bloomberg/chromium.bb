// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"

#include "base/test/scoped_feature_list.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class MockChromePasswordProtectionService
    : public ChromePasswordProtectionService {
 public:
  MockChromePasswordProtectionService()
      : ChromePasswordProtectionService(),
        is_incognito_(false),
        is_extended_reporting_(false),
        is_history_sync_enabled_(false) {}
  bool IsExtendedReporting() override { return is_extended_reporting_; }
  bool IsIncognito() override { return is_incognito_; }
  bool IsHistorySyncEnabled() override { return is_history_sync_enabled_; }

  // Configures the results returned by IsExtendedReporting(), IsIncognito(),
  // and IsHistorySyncEnabled().
  void ConfigService(bool is_incognito,
                     bool is_extended_reporting,
                     bool is_history_sync_enabled) {
    is_incognito_ = is_incognito;
    is_extended_reporting_ = is_extended_reporting;
    is_history_sync_enabled_ = is_history_sync_enabled;
  }

 private:
  bool is_incognito_;
  bool is_extended_reporting_;
  bool is_history_sync_enabled_;
};

class ChromePasswordProtectionServiceTest : public testing::Test {
 public:
  typedef std::map<std::string, std::string> Parameters;
  ChromePasswordProtectionServiceTest() {}

  // Sets up Finch trial feature parameters.
  void SetFeatureParams(const base::Feature& feature,
                        const std::string& trial_name,
                        const Parameters& params) {
    static std::set<std::string> features = {feature.name};
    params_manager_.ClearAllVariationParams();
    params_manager_.SetVariationParamsWithFeatureAssociations(trial_name,
                                                              params, features);
  }

  // Creates Finch trial parameters.
  Parameters CreateParameters(bool allowed_for_incognito,
                              bool allowed_for_all,
                              bool allowed_for_extended_reporting,
                              bool allowed_for_history_sync) {
    return {{"incognito", allowed_for_incognito ? "true" : "false"},
            {"all_population", allowed_for_all ? "true" : "false"},
            {"extended_reporting",
             allowed_for_extended_reporting ? "true" : "false"},
            {"history_sync", allowed_for_history_sync ? "true" : "false"}};
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  variations::testing::VariationParamsManager params_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingSBEROnlyNoIncognito) {
  MockChromePasswordProtectionService service;
  // By default kPasswordFieldOnFocusPinging feature is disabled.
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "SBEROnlyNoIncognito",
  // "params": {
  //     "incognito": "false",
  //     "extended_reporting": "true",
  //     "history_sync": "false"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters sber_and_no_incognito =
      CreateParameters(false, false, true, false);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "SBEROnlyNoIncognito",
                   sber_and_no_incognito);
  service.ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingSBERAndHistorySyncNoIncognito) {
  MockChromePasswordProtectionService service;
  // By default kPasswordFieldOnFocusPinging feature is disabled.
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "SBERAndHistorySyncNoIncognito",
  // "params": {
  //     "incognito": "false",
  //     "extended_reporting": "true",
  //     "history_sync": "true"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters sber_and_sync_no_incognito =
      CreateParameters(false, false, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging,
                   "SBERAndHistorySyncNoIncognito", sber_and_sync_no_incognito);
  service.ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingAllButNoIncognito) {
  MockChromePasswordProtectionService service;
  // By default kPasswordFieldOnFocusPinging feature is disabled.
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "AllButNoIncognito",
  // "params": {
  //     "all_population": "true",
  //     "incongito": "false"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters all_users = CreateParameters(false, true, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "AllButNoIncognito",
                   all_users);
  service.ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
}

TEST_F(ChromePasswordProtectionServiceTest,
       VerifyFinchControlForLowReputationPingAll) {
  MockChromePasswordProtectionService service;
  // By default kPasswordFieldOnFocusPinging feature is disabled.
  EXPECT_FALSE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));

  // Enables kPasswordFieldOnFocusPinging feature.
  scoped_feature_list_.InitAndEnableFeature(kPasswordFieldOnFocusPinging);
  // Creates finch trial parameters correspond to the following experiment:
  // "name": "All",
  // "params": {
  //     "all_population": "true",
  //     "incognito": "true"
  // },
  // "enable_features": [
  //     "PasswordFieldOnFocusPinging"
  // ]
  Parameters all_users = CreateParameters(true, true, true, true);
  SetFeatureParams(kPasswordFieldOnFocusPinging, "All", all_users);
  service.ConfigService(false /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(false /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, false /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, false /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
  service.ConfigService(true /*incognito*/, true /*SBER*/, true /*sync*/);
  EXPECT_TRUE(service.IsPingingEnabled(kPasswordFieldOnFocusPinging));
}

}  // namespace safe_browsing
