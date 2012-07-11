// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_BROWSERTEST_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace options2 {

class OptionsBrowserTest : public InProcessBrowserTest {
 public:
  OptionsBrowserTest();

  // Navigate to the settings tab and block until complete.
  void NavigateToSettings();

  // Check navbar's existence.
  void VerifyNavbar();

  // Verify that the page title is correct.
  // The only guarantee we can make about the title of a settings tab is that
  // it should contain IDS_SETTINGS_TITLE somewhere.
  void VerifyTitle();
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI2_BROWSERTEST_H_
