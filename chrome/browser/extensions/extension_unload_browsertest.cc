// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/window_open_disposition.h"

namespace extensions {

using ExtensionUnloadBrowserTest = ExtensionBrowserTest;

IN_PROC_BROWSER_TEST_F(ExtensionUnloadBrowserTest, TestUnload) {
  // Load an extension that installs unload and beforeunload listeners.
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("unload_listener"));
  ASSERT_TRUE(extension);
  std::string id = extension->id();
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  GURL initial_tab_url =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetLastCommittedURL();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension->GetResourceURL("page.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());
  extension_service()->UnloadExtension(id,
                                       UnloadedExtensionInfo::REASON_DISABLE);
  // There should only be one remaining web contents - the initial one.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(
      initial_tab_url,
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetLastCommittedURL());
}

// TODO(devlin): Investigate what to do for embedded iframes.

}  // namespace extensions
