// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_TEST_H_

#include "chrome/test/base/web_ui_browsertest.h"

class CommandLine;

class ManagedUserSettingsTest : public WebUIBrowserTest {
 public:
  ManagedUserSettingsTest();
  virtual ~ManagedUserSettingsTest();

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedUserSettingsTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_MANAGED_USER_SETTINGS_TEST_H_
