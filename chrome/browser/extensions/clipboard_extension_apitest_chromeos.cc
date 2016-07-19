// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

class ClipboardExtensionApiTest : public ExtensionApiTest {};

}  // namespace

IN_PROC_BROWSER_TEST_F(ClipboardExtensionApiTest, ClipboardDataChanged) {
  ExtensionTestMessageListener result_listener("success 2", false);
  ASSERT_TRUE(RunPlatformAppTest("clipboard/clipboard_data_changed"))
      << message_;
  ASSERT_TRUE(result_listener.WaitUntilSatisfied());
}
