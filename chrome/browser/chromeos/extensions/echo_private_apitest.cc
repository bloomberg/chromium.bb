// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/echo_private_api.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/ui/echo_dialog_view.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chromeos/chromeos_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace utils = extension_function_test_utils;

namespace chromeos {

class ExtensionEchoPrivateApiTest : public ExtensionApiTest {
 public:
  enum DialogTestAction {
    DIALOG_TEST_ACTION_NONE,
    DIALOG_TEST_ACTION_ACCEPT,
    DIALOG_TEST_ACTION_CANCEL,
  };

  ExtensionEchoPrivateApiTest()
      : expected_dialog_buttons_(ui::DIALOG_BUTTON_NONE),
        dialog_action_(DIALOG_TEST_ACTION_NONE),
        dialog_invocation_count_(0) {
  }

  virtual ~ExtensionEchoPrivateApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);

    // Force usage of stub cros settings provider instead of device settings
    // provider.
    command_line->AppendSwitch(switches::kStubCrosSettings);
  }

  void RunDefaultGetUserFunctionAndExpectResultEquals(bool expected_result) {
    scoped_refptr<EchoPrivateGetUserConsentFunction> function(
        EchoPrivateGetUserConsentFunction::CreateForTest(base::Bind(
            &ExtensionEchoPrivateApiTest::OnDialogShown, this)));
    function->set_has_callback(true);

    scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
        function.get(),
        "[{\"serviceName\":\"some_name\",\"origin\":\"http://chromium.org\"}]",
        browser()));

    ASSERT_TRUE(result.get());
    ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());

    bool result_as_boolean = false;
    ASSERT_TRUE(result->GetAsBoolean(&result_as_boolean));

    EXPECT_EQ(expected_result, result_as_boolean);
  }

  void OnDialogShown(chromeos::EchoDialogView* dialog) {
    dialog_invocation_count_++;
    ASSERT_LE(dialog_invocation_count_, 1);

    EXPECT_EQ(expected_dialog_buttons_, dialog->GetDialogButtons());

    // Don't accept the dialog if the dialog buttons don't match expectation.
    // Accepting a dialog which should not have accept option may crash the
    // test. The test already failed, so it's ok to cancel the dialog.
    DialogTestAction dialog_action = dialog_action_;
    if (dialog_action == DIALOG_TEST_ACTION_ACCEPT &&
        expected_dialog_buttons_ != dialog->GetDialogButtons()) {
      dialog_action = DIALOG_TEST_ACTION_CANCEL;
    }

    // Perform test action on the dialog.
    // The dialog should stay around until AcceptWindow or CancelWindow is
    // called, so base::Unretained is safe.
    if (dialog_action == DIALOG_TEST_ACTION_ACCEPT) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&chromeos::EchoDialogView::Accept),
                     base::Unretained(dialog)));
    } else if (dialog_action == DIALOG_TEST_ACTION_CANCEL) {
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(base::IgnoreResult(&chromeos::EchoDialogView::Cancel),
                     base::Unretained(dialog)));
    }
  }

  int dialog_invocation_count() const {
    return dialog_invocation_count_;
  }

 protected:
  int expected_dialog_buttons_;
  DialogTestAction dialog_action_;

 private:
  int dialog_invocation_count_;
};

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest, EchoTest) {
  EXPECT_TRUE(RunComponentExtensionTest("echo/component_extension"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_InvalidOrigin) {
  expected_dialog_buttons_ =  ui::DIALOG_BUTTON_NONE;
  dialog_action_ = DIALOG_TEST_ACTION_NONE;

  scoped_refptr<EchoPrivateGetUserConsentFunction> function(
      EchoPrivateGetUserConsentFunction::CreateForTest(base::Bind(
          &ExtensionEchoPrivateApiTest::OnDialogShown,
          base::Unretained(this))));

  std::string error = utils::RunFunctionAndReturnError(
      function.get(),
      "[{\"serviceName\":\"some name\",\"origin\":\"invalid\"}]",
      browser());

  EXPECT_EQ("Invalid origin.", error);
  EXPECT_EQ(0, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefNotSet) {
  expected_dialog_buttons_ = ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  dialog_action_ = DIALOG_TEST_ACTION_ACCEPT;

  RunDefaultGetUserFunctionAndExpectResultEquals(true);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefTrue) {
  chromeos::CrosSettings::Get()->SetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, true);

  expected_dialog_buttons_ = ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  dialog_action_ = DIALOG_TEST_ACTION_ACCEPT;

  RunDefaultGetUserFunctionAndExpectResultEquals(true);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_ConsentDenied) {
  chromeos::CrosSettings::Get()->SetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, true);

  expected_dialog_buttons_ = ui::DIALOG_BUTTON_CANCEL | ui::DIALOG_BUTTON_OK;
  dialog_action_ = DIALOG_TEST_ACTION_CANCEL;

  RunDefaultGetUserFunctionAndExpectResultEquals(false);

  EXPECT_EQ(1, dialog_invocation_count());
}

IN_PROC_BROWSER_TEST_F(ExtensionEchoPrivateApiTest,
                       GetUserConsent_AllowRedeemPrefFalse) {
  chromeos::CrosSettings::Get()->SetBoolean(
            chromeos::kAllowRedeemChromeOsRegistrationOffers, false);

  expected_dialog_buttons_ = ui::DIALOG_BUTTON_CANCEL;
  dialog_action_ = DIALOG_TEST_ACTION_CANCEL;

  RunDefaultGetUserFunctionAndExpectResultEquals(false);

  EXPECT_EQ(1, dialog_invocation_count());
}

}  // namespace chromeos
