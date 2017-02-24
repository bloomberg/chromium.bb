// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace content {

class MediaSessionServiceImplBrowserTest : public ContentBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("--enable-blink-features", "MediaSession");
  }
};

// Two windows from the same BrowserContext.
IN_PROC_BROWSER_TEST_F(MediaSessionServiceImplBrowserTest,
                       CrashMessageOnUnload) {
  NavigateToURL(shell(), GetTestUrl("media/session", "embedder.html"));
  // Navigate to a chrome:// URL to avoid render process re-use.
  NavigateToURL(shell(), GURL("chrome://flags"));
  // Should not crash.
}

}  // namespace content
