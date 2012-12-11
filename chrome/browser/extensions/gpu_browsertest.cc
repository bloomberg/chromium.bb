// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/glue/webpreferences.h"

// Tests that GPU acceleration is disabled for extension background
// pages. See crbug.com/163698 .
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebKitPrefsBackgroundPage) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(browser()->profile())->process_manager();
  extensions::ExtensionHost* host =
      FindHostWithPath(manager, "/backgroundpage.html", 1);
  webkit_glue::WebPreferences prefs =
      host->render_view_host()->GetWebkitPreferences();
  ASSERT_FALSE(prefs.accelerated_compositing_enabled);
}
