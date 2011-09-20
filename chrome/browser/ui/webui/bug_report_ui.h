// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_BUG_REPORT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_BUG_REPORT_UI_H_

#include <string>

#include "chrome/browser/ui/webui/html_dialog_ui.h"

class Browser;
class TabContents;

namespace browser {
void ShowHtmlBugReportView(Browser* browser,
                           const std::string& description_template,
                           size_t issue_type);
}  // namespace browser

class BugReportUI : public HtmlDialogUI {
 public:
  explicit BugReportUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(BugReportUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_BUG_REPORT_UI_H_
