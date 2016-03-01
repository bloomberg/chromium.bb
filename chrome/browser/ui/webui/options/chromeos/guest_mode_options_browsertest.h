// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_GUEST_MODE_OPTIONS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_GUEST_MODE_OPTIONS_BROWSERTEST_H_

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"

// This is a helper class used by guest_mode_options_browsertest.js to enable
// guest mode.
class GuestModeOptionsUIBrowserTest : public WebUIBrowserTest {
 public:
  GuestModeOptionsUIBrowserTest();
  ~GuestModeOptionsUIBrowserTest() override;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  DISALLOW_COPY_AND_ASSIGN(GuestModeOptionsUIBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_CHROMEOS_GUEST_MODE_OPTIONS_BROWSERTEST_H_
