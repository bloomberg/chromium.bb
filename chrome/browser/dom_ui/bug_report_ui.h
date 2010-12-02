// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_
#define CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_

#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/window.h"

namespace gfx {
class Rect;
}  // namespace gfx

class TabContents;
class NSWindow;


// TODO(rkc): The following code is very ugly and needs to be refactored.
// http://code.google.com/p/chromium/issues/detail?id=65119
namespace browser {
#if defined(TOOLKIT_VIEWS)
void ShowHtmlBugReportView(views::Window* parent, Browser* browser);
#elif defined(OS_LINUX)
void ShowHtmlBugReportView(gfx::NativeWindow window, const gfx::Rect& bounds,
                           Browser* browser);
#elif defined(OS_MACOSX)
void ShowHtmlBugReportView(NSWindow* window, Browser* browser);
#endif
}  // namespace browser

class BugReportUI : public HtmlDialogUI {
 public:
  explicit BugReportUI(TabContents* contents);
 private:

  DISALLOW_COPY_AND_ASSIGN(BugReportUI);
};

#endif  // CHROME_BROWSER_DOM_UI_BUG_REPORT_UI_H_
