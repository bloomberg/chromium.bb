// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_install_dialog.h"
#include "chrome/browser/extensions/extension_management_api.h"
#include "chrome/browser/extensions/extension_management_api_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"

namespace keys = extension_management_api_constants;
namespace util = extension_function_test_utils;

class ExtensionManagementApiBrowserTest : public ExtensionBrowserTest {};

// We test this here instead of in an ExtensionApiTest because normal extensions
// are not allowed to call the install function.
IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, InstallEvent) {
  ExtensionTestMessageListener listener1("ready", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/install_event")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  ExtensionTestMessageListener listener2("got_event", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test/management/enabled_extension")));
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest, LaunchApp) {
  ExtensionTestMessageListener listener1("app_launched", false);
  ExtensionTestMessageListener listener2("got_expected_error", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/simple_extension")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/packaged_app")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
  ASSERT_TRUE(listener2.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiBrowserTest,
                       LaunchAppFromBackground) {
  ExtensionTestMessageListener listener1("success", false);
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/packaged_app")));
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("management/launch_app_from_background")));
  ASSERT_TRUE(listener1.WaitUntilSatisfied());
}

class ExtensionManagementApiEscalationTest : public ExtensionBrowserTest {
 protected:
  // The id of the permissions escalation test extension we use.
  static const char kId[];

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionService* service = browser()->profile()->GetExtensionService();

    // Install low-permission version of the extension.
    ASSERT_TRUE(InstallExtension(
        test_data_dir_.AppendASCII("permissions-low-v1.crx"), 1));
    EXPECT_TRUE(service->GetExtensionById(kId, false) != NULL);

    // Update to a high-permission version - it should get disabled.
    EXPECT_TRUE(UpdateExtension(
        kId, test_data_dir_.AppendASCII("permissions-high-v2.crx"), -1));
    EXPECT_TRUE(service->GetExtensionById(kId, false) == NULL);
    EXPECT_TRUE(service->GetExtensionById(kId, true) != NULL);
    EXPECT_TRUE(
        service->extension_prefs()->DidExtensionEscalatePermissions(kId));
  }

  void ReEnable(bool user_gesture, const std::string& expected_error) {
    scoped_refptr<SetEnabledFunction> function(new SetEnabledFunction);
    if (user_gesture)
      function->set_user_gesture(true);
    bool response = util::RunAsyncFunction(
        function.get(),
        base::StringPrintf("[\"%s\", true]", kId),
        browser(),
        util::NONE);
    if (expected_error.empty()) {
      EXPECT_EQ(true, response);
    } else {
      EXPECT_TRUE(response == false);
      EXPECT_EQ(expected_error, function->GetError());
    }
  }

};

const char ExtensionManagementApiEscalationTest::kId[] =
    "pgdpcfcocojkjfbgpiianjngphoopgmo";

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       DisabledReason) {
  scoped_ptr<base::Value> result(
      util::RunFunctionAndReturnResult(new GetExtensionByIdFunction(),
                                       base::StringPrintf("[\"%s\"]", kId),
                                       browser()));
  ASSERT_TRUE(result.get() != NULL);
  ASSERT_TRUE(result->IsType(base::Value::TYPE_DICTIONARY));
  base::DictionaryValue* dict =
      static_cast<base::DictionaryValue*>(result.get());
  std::string reason;
  EXPECT_TRUE(dict->GetStringASCII(keys::kDisabledReasonKey, &reason));
  EXPECT_EQ(reason, std::string(keys::kDisabledReasonPermissionsIncrease));
}

IN_PROC_BROWSER_TEST_F(ExtensionManagementApiEscalationTest,
                       ReEnable) {
  // Expect an error about no gesture.
  ReEnable(false, keys::kGestureNeededForEscalationError);

  // Expect an error that user cancelled the dialog.
  SetExtensionInstallDialogAutoConfirmForTests(false);
  ReEnable(true, keys::kUserDidNotReEnableError);

  // This should succeed when user accepts dialog.
  SetExtensionInstallDialogAutoConfirmForTests(true);
  ReEnable(true, "");
}
