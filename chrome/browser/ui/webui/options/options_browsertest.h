// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_BROWSERTEST_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/web_ui_message_handler.h"

// This is a helper class used by options_browsertest.js to feed the navigation
// history back to the test.
class OptionsBrowserTest : public WebUIBrowserTest,
                           public content::WebUIMessageHandler {
 public:
  OptionsBrowserTest();
  virtual ~OptionsBrowserTest();

 protected:
  // Clears the preference at the given |path|.
  void ClearPref(const char* path);

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // WebUIBrowserTest implementation.
  virtual content::WebUIMessageHandler* GetMockMessageHandler() OVERRIDE;

  // A callback for the 'optionsTestReportHistory' message, this sends the
  // URLs in the "back" tab history, including the current entry, back to the
  // WebUI via a callback.
  void ReportHistory(const base::ListValue* list_value);

  // A callback for the 'optionsTestSetPref' message. The first argument should
  // be a pref path (e.g., "profile.managed_users"); the second, the new value
  // to set for that pref.
  void HandleSetPref(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(OptionsBrowserTest);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_OPTIONS_BROWSERTEST_H_
