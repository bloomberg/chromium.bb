// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/remoting/remote_desktop_browsertest.h"

namespace remoting {

class UnauthenticatedBrowserTest : public RemoteDesktopBrowserTest {
};

IN_PROC_BROWSER_TEST_F(UnauthenticatedBrowserTest, MANUAL_Unauthenticated) {
  Install();
  LaunchChromotingApp(true);
  LoadBrowserTestJavaScript(app_web_content());

  content::WebContents* content = app_web_content();
  LoadScript(content, FILE_PATH_LITERAL("unauthenticated_browser_test.js"));
  RunJavaScriptTest(content, "Unauthenticated", "{}");

  Cleanup();
}

}  // namespace remoting
