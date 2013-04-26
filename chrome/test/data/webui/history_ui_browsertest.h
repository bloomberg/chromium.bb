// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_

#include "chrome/test/base/web_ui_browsertest.h"

class HistoryUIBrowserTest : public WebUIBrowserTest {
 public:
  HistoryUIBrowserTest();
  virtual ~HistoryUIBrowserTest();

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed);

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryUIBrowserTest);
};

#endif  // CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
