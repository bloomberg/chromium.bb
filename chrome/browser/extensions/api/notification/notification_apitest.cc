// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification/notification_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/common/chrome_switches.h"

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

class NotificationApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestNormalNotification) {
  scoped_refptr<extensions::NotificationShowFunction>
      notification_show_function(new extensions::NotificationShowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_show_function->set_extension(empty_extension.get());
  notification_show_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_show_function,
      "[{"
      "\"iconUrl\": \"http://www.google.com/intl/en/chrome/assets/"
      "common/images/chrome_logo_2x.png\","
      "\"title\": \"Attention!\","
      "\"message\": \"Check out Cirque du Soleil\","
      "\"replaceId\": \"12345678\""
      "}]",
      browser(), utils::NONE));

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
}
