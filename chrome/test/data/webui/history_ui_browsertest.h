// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_

#include "base/time/time.h"

#include "chrome/test/base/web_ui_browser_test.h"

class HistoryService;

class HistoryUIBrowserTest : public WebUIBrowserTest {
 public:
  HistoryUIBrowserTest();
  virtual ~HistoryUIBrowserTest();

  virtual void SetUpOnMainThread() OVERRIDE;

 protected:
  // Sets the pref to allow or prohibit deleting history entries.
  void SetDeleteAllowed(bool allowed);

  // Add a test entry to the history database. The timestamp for the entry will
  // be |hour_offset| hours after |baseline_time_|.
  void AddPageToHistory(
      int hour_offset, const std::string& url, const std::string& title);

  // Clears kAcceptLanguages pref value.
  void ClearAcceptLanguages();

 private:
  // The HistoryService is owned by the profile.
  HistoryService* history_;

  // The time from which entries added via AddPageToHistory() will be offset.
  base::Time baseline_time_;

  // Counter used to generate a unique ID for each page added to the history.
  int32 page_id_;

  DISALLOW_COPY_AND_ASSIGN(HistoryUIBrowserTest);
};

#endif  // CHROME_TEST_DATA_WEBUI_HISTORY_UI_BROWSERTEST_H_
