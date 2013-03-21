// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/common/chrome_switches.h"

namespace utils = extension_function_test_utils;

namespace {

class ExtensionEchoPrivateApiTest : public ExtensionApiTest {
 public:
  ExtensionEchoPrivateApiTest() {}
  virtual ~ExtensionEchoPrivateApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);

    // Force usage of stub cros settings provider instead of device settings
    // provider.
    command_line->AppendSwitch(switches::kStubCrosSettings);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, EchoTest) {
  EXPECT_TRUE(RunComponentExtensionTest("echo/component_extension"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_InvalidOrigin) {
  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
        new EchoPrivateGetUserConsentFunction());

  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      "[{\"serviceName\":\"some name\",\"origin\":\"invalid\"}]",
      browser());

  EXPECT_EQ("Invalid origin.", error);
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefNotSet) {
  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      new EchoPrivateGetUserConsentFunction());

  function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"serviceName\":\"some name\",\"origin\":\"http://chromium.org\"}]",
      browser()));

  ASSERT_TRUE(result.get());

  bool result_as_boolean = false;
  EXPECT_TRUE(result->GetAsBoolean(&result_as_boolean));
  EXPECT_FALSE(result_as_boolean);

  EXPECT_EQ(!g_browser_process->browser_policy_connector()
                ->IsEnterpriseManaged(),
            function->redeem_offers_allowed());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefTrue) {
  chromeos::CrosSettings::Get()->SetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, true);

  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      new EchoPrivateGetUserConsentFunction());

  function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"serviceName\":\"some_name\",\"origin\":\"http://chromium.org\"}]",
      browser()));

  ASSERT_TRUE(result.get());

  bool result_as_boolean = false;
  EXPECT_TRUE(result->GetAsBoolean(&result_as_boolean));
  EXPECT_FALSE(result_as_boolean);

  EXPECT_TRUE(function->redeem_offers_allowed());
}


IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefFalse) {
  chromeos::CrosSettings::Get()->SetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, false);

  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      new EchoPrivateGetUserConsentFunction());

  function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      function.get(),
      "[{\"serviceName\":\"some_name\",\"origin\":\"http://chromium.org\"}]",
      browser()));

  ASSERT_TRUE(result.get());

  bool result_as_boolean = false;
  EXPECT_TRUE(result->GetAsBoolean(&result_as_boolean));
  EXPECT_FALSE(result_as_boolean);

  EXPECT_FALSE(function->redeem_offers_allowed());
}


}  // namespace
