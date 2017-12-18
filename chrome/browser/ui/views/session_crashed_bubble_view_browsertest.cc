// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <string>

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "ui/views/view.h"

class SessionCrashedBubbleViewTest : public DialogBrowserTest {
 public:
  SessionCrashedBubbleViewTest() {}
  ~SessionCrashedBubbleViewTest() override {}

  void ShowUi(const std::string& name) override {
    views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser())
                                   ->toolbar()
                                   ->app_menu_button();
    SessionCrashedBubbleView* crash_bubble =
        new SessionCrashedBubbleView(anchor_view, browser(), false);
    views::BubbleDialogDelegateView::CreateBubble(crash_bubble)->Show();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionCrashedBubbleViewTest);
};

IN_PROC_BROWSER_TEST_F(SessionCrashedBubbleViewTest,
                       InvokeUi_SessionCrashedBubble) {
  ShowAndVerifyUi();
}
