// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/render_view_host.h"
#include "webkit/glue/webpreferences.h"

// Tests that GPU-related WebKit preferences are set for extension background
// pages. See http://crbug.com/64512.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebKitPrefsBackgroundPage) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindHostWithPath(manager, "/backgroundpage.html", 1);
  webkit_glue::WebPreferences prefs =
      host->render_view_host()->GetWebkitPreferences();
  ASSERT_TRUE(prefs.experimental_webgl_enabled);
  ASSERT_TRUE(prefs.accelerated_compositing_enabled);
  ASSERT_TRUE(prefs.accelerated_2d_canvas_enabled);
}
