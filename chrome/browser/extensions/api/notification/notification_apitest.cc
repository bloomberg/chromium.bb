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

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestSimpleNotification) {
  scoped_refptr<extensions::NotificationShowFunction>
      notification_show_function(new extensions::NotificationShowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_show_function->set_extension(empty_extension.get());
  notification_show_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_show_function,
      "[{"
      "\"type\": \"simple\","
      "\"iconUrl\": \"http://www.google.com/intl/en/chrome/assets/"
      "common/images/chrome_logo_2x.png\","
      "\"title\": \"Attention!\","
      "\"message\": \"Check out Cirque du Soleil\","
      "\"replaceId\": \"12345678\""
      "}]",
      browser(), utils::NONE));

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());

  // TODO(miket): confirm that the show succeeded.
}

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestBaseFormatNotification) {
  scoped_refptr<extensions::NotificationShowFunction>
      notification_show_function(new extensions::NotificationShowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_show_function->set_extension(empty_extension.get());
  notification_show_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_show_function,
      "[{"
      "\"type\": \"base\","
      "\"iconUrl\": \"http://www.google.com/intl/en/chrome/assets/"
      "common/images/chrome_logo_2x.png\","
      "\"title\": \"Attention!\","
      "\"message\": \"Check out Cirque du Soleil\","
      "\"priority\": 1,"
      "\"timestamp\": \"Tue, 15 Nov 1994 12:45:26 GMT\","
      "\"secondIconUrl\": \"http://www.google.com/logos/2012/"
      "Day-Of-The-Dead-12-hp.jpg\","
      "\"unreadCount\": 42,"
      "\"buttonOneTitle\": \"Up\","
      "\"buttonTwoTitle\": \"Down\","
      "\"expandedMessage\": \"This is a longer expanded message.\","
      "\"imageUrl\": \"http://www.google.com/logos/2012/election12-hp.jpg\","
      "\"replaceId\": \"12345678\""
      "}]",
      browser(), utils::NONE));

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());

  // TODO(miket): confirm that the show succeeded.
}

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestMultipleItemNotification) {
  scoped_refptr<extensions::NotificationShowFunction>
      notification_show_function(new extensions::NotificationShowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_show_function->set_extension(empty_extension.get());
  notification_show_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_show_function,
      "[{"
      "\"type\": \"multiple\","
      "\"iconUrl\": \"https://code.google.com/p/chromium/logo\","
      "\"title\": \"Multiple Item Notification Title\","
      "\"message\": \"Multiple item notification message.\","
      "\"items\": ["
      "  {\"title\": \"Brett Boe\","
      " \"message\": \"This is an important message!\"},"
      "  {\"title\": \"Carla Coe\","
      " \"message\": \"Just took a look at the proposal\"},"
      "  {\"title\": \"Donna Doe\","
      " \"message\": \"I see that you went to the conference\"},"
      "  {\"title\": \"Frank Foe\","
      " \"message\": \"I ate Harry's sandwich!\"},"
      "  {\"title\": \"Grace Goe\","
      " \"message\": \"I saw Frank steal a sandwich :-)\"}"
      "],"
      "\"priority\": 1,"
      "\"timestamp\": \"Fri, 16 Nov 2012 01:17:15 GMT\","
      "\"replaceId\": \"12345678\""
      "}]",
      browser(), utils::NONE));
  // TODO(dharcourt): [...], items = [{title: foo, message: bar}, ...], [...]

  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());

  // TODO(dharcourt): confirm that the show succeeded.
}

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestEvents) {
  ASSERT_TRUE(RunExtensionTest("notification/api/events")) << message_;
}
