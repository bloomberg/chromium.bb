// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI_UITEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI_UITEST_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/test/ui/ui_test.h"

class TabProxy;

class OptionsUITest : public UITest {
 public:
  OptionsUITest();

  // Navigate to the settings tab and block until complete.
  void NavigateToSettings(scoped_refptr<TabProxy> tab);

  // Check navbar's existence.
  void VerifyNavbar(scoped_refptr<TabProxy> tab);

  // Verify that the page title is correct.
  // The only guarantee we can make about the title of a settings tab is that
  // it should contain IDS_SETTINGS_TITLE somewhere.
  void VerifyTitle(scoped_refptr<TabProxy> tab);

  // Check section headers in navbar.
  // For ChromeOS, there should be 1 + 7:
  //   Search, Basics, Personal, System, Internet, Under the Hood,
  //   Users and Extensions.
  // For other platforms, there should 1 + 4:
  //   Search, Basics, Personal, Under the Hood and Extensions.
  void VerifySections(scoped_refptr<TabProxy> tab);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_OPTIONS_UI_UITEST_H_
