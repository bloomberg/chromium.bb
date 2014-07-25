// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/test/remoting/remote_desktop_browsertest.h"
#include "chrome/test/remoting/waiter.h"

namespace remoting {

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Cancel_PIN) {
  SetUpTestForMe2Me();

  content::WebContents* content = app_web_content();
  LoadScript(content, FILE_PATH_LITERAL("cancel_pin_browser_test.js"));

  RunJavaScriptTest(content, "Cancel_PIN", "{"
    "pin: '" + me2me_pin() + "'"
  "}");

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Invalid_PIN) {
  SetUpTestForMe2Me();

  content::WebContents* content = app_web_content();
  LoadScript(content, FILE_PATH_LITERAL("invalid_pin_browser_test.js"));

  RunJavaScriptTest(content, "Invalid_PIN", "{"
    // Append arbitrary characters after the pin to make it invalid.
    "pin: '" + me2me_pin() + "ABC'"
  "}");

  Cleanup();
}

IN_PROC_BROWSER_TEST_F(RemoteDesktopBrowserTest, MANUAL_Update_PIN) {
  SetUpTestForMe2Me();

  content::WebContents* content = app_web_content();
  LoadScript(content, FILE_PATH_LITERAL("update_pin_browser_test.js"));

  RunJavaScriptTest(content, "Update_PIN", "{"
    "old_pin: '" + me2me_pin() + "',"
    "new_pin: '314159'"
  "}");

  Cleanup();
}

}  // namespace remoting
