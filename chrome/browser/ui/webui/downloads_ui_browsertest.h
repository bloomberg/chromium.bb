// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_BROWSERTEST_H_

#include "chrome/test/base/web_ui_browser_test.h"

// This is a helper class used by downloads_ui_browsertest.js.
class DownloadsUIBrowserTest : public WebUIBrowserTest {
 public:
  DownloadsUIBrowserTest();
  ~DownloadsUIBrowserTest() override;

  // WebUIBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_BROWSERTEST_H_
