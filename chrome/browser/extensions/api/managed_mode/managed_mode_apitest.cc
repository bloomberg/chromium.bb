// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"

// Tests enabling and querying managed mode.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ManagedModeGetAndEnable) {
  ASSERT_FALSE(ManagedMode::IsInManagedMode());

  ASSERT_TRUE(RunComponentExtensionTest("managed_mode/get_enter")) << message_;

  EXPECT_TRUE(ManagedMode::IsInManagedMode());
}

// Tests the event when entering or leaving managed mode.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ManagedModeOnChange) {
  ASSERT_FALSE(ManagedMode::IsInManagedMode());

  // We can't just call RunComponentExtension() like above, because we need to
  // fire the event while the page is waiting.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("managed_mode/on_change");
  const extensions::Extension* extension =
      LoadExtensionAsComponent(extension_path);
  ASSERT_TRUE(extension) << "Failed to load extension.";

  ResultCatcher catcher;
  // Tell the test what values for the |onChange| event to expect.
  std::string page_url = "test.html?expect=true,false";
  ui_test_utils::NavigateToURL(browser(),
                               extension->GetResourceURL(page_url));

  // Fire the extension event when entering managed mode. We directly call
  // SetInManagedMode() to bypass any confirmation dialogs etc.
  ManagedMode::GetInstance()->SetInManagedMode(browser()->profile());

  // Fire the extension event when leaving managed mode.
  ManagedMode::GetInstance()->SetInManagedMode(NULL);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}
