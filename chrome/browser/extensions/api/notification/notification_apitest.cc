// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/notification/notification_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

class NotificationApiTest : public PlatformAppApiTest {
};

}  // namespace

IN_PROC_BROWSER_TEST_F(NotificationApiTest, TestNothing) {
  scoped_refptr<extensions::NotificationShowFunction>
      notification_show_function(new extensions::NotificationShowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  notification_show_function->set_extension(empty_extension.get());
  notification_show_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      notification_show_function,
      "[{\"text\": \"Check out Cirque du Soleil\"}]",
      browser(), utils::NONE));
  ASSERT_EQ(base::Value::TYPE_BOOLEAN, result->GetType());
}
