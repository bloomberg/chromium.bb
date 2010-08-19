// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_
#define CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_

#include "chrome/browser/dom_ui/html_dialog_ui.h"

class TabContents;

class BugReportUI : public HtmlDialogUI {
 public:
  explicit BugReportUI(TabContents* contents);
 private:

  DISALLOW_COPY_AND_ASSIGN(BugReportUI);
};

#endif  // CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_
