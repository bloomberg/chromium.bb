// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_SUPERVISED_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_SUPERVISED_BROWSERTEST_H_

#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"

class DownloadsWebUIForSupervisedUsersTest : public DownloadsUIBrowserTest {
 public:
  // InProcessBrowserTest implementation.
  void SetUpCommandLine(base::CommandLine* command_line) override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_DOWNLOADS_UI_SUPERVISED_BROWSERTEST_H_
