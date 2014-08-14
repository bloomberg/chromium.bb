// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification_provider/notification_provider_api.h"

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/notification_provider.h"

typedef ExtensionApiTest NotificationProviderApiTest;

namespace {

void CreateNotificationOptionsForTest(
    extensions::api::notifications::NotificationOptions* options) {
  options->type = extensions::api::notifications::ParseTemplateType("basic");
  options->icon_url = scoped_ptr<std::string>(new std::string("icon.png"));
  options->title = scoped_ptr<std::string>(new std::string("Title"));
  options->message =
      scoped_ptr<std::string>(new std::string("Here goes the message"));
  return;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationProviderApiTest, Events) {
  std::string sender_id1 = "SenderId1";
  std::string notification_id1 = "NotificationId1";

  scoped_ptr<extensions::api::notifications::NotificationOptions> options(
      new extensions::api::notifications::NotificationOptions());
  CreateNotificationOptionsForTest(options.get());

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  // Test notification provider extension
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("notification_provider/events"));
  ASSERT_TRUE(extension);

  scoped_ptr<extensions::NotificationProviderEventRouter> event_router(
      new extensions::NotificationProviderEventRouter(browser()->profile()));

  event_router->CreateNotification(
      extension->id(), sender_id1, notification_id1, *options);
  event_router->UpdateNotification(
      extension->id(), sender_id1, notification_id1, *options);
  event_router->ClearNotification(
      extension->id(), sender_id1, notification_id1);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(NotificationProviderApiTest, TestBasicUsage) {
  ASSERT_TRUE(RunExtensionTest("notification_provider/basic_usage"))
      << message_;
}
