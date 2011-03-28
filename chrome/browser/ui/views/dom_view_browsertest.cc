// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

using namespace views;

class DOMViewTest : public InProcessBrowserTest {
 public:
  Widget* CreatePopupWindow() {
    Widget::CreateParams params(Widget::CreateParams::TYPE_POPUP);
    params.mirror_origin_in_rtl = false;
    Widget* widget = Widget::CreateWidget(params);
    widget->Init(NULL, gfx::Rect(0, 0, 400, 400));
    return widget;
  }
};

// Tests if creating and deleting dom_view
// does not crash and leak memory.
IN_PROC_BROWSER_TEST_F(DOMViewTest, TestShowAndHide) {
  Widget* one = CreatePopupWindow();

  DOMView* dom_view = new DOMView();
  one->GetRootView()->AddChildView(dom_view);

  dom_view->Init(browser()->profile(), NULL);
  dom_view->LoadURL(GURL("http://www.google.com"));
  ui_test_utils::WaitForNotification(NotificationType::LOAD_STOP);
  one->Show();

  ui_test_utils::RunAllPendingInMessageLoop();

  one->Hide();
}

// Tests if removing from tree then deleting dom_view
// does not crash and leak memory.
IN_PROC_BROWSER_TEST_F(DOMViewTest, TestRemoveAndDelete) {
  Widget* one = CreatePopupWindow();

  DOMView* dom_view = new DOMView();
  one->GetRootView()->AddChildView(dom_view);

  dom_view->Init(browser()->profile(), NULL);
  dom_view->LoadURL(GURL("http://www.google.com"));
  ui_test_utils::WaitForNotification(NotificationType::LOAD_STOP);
  one->Show();

  ui_test_utils::RunAllPendingInMessageLoop();

  one->GetRootView()->RemoveChildView(dom_view);

  delete dom_view;

  one->Hide();
}

// Tests if reparenting dom_view does not crash and does not leak
// memory.
IN_PROC_BROWSER_TEST_F(DOMViewTest, TestReparent) {
  Widget* one = CreatePopupWindow();

  DOMView* dom_view = new DOMView();
  one->GetRootView()->AddChildView(dom_view);

  dom_view->Init(browser()->profile(), NULL);
  dom_view->LoadURL(GURL("http://www.google.com"));
  ui_test_utils::WaitForNotification(NotificationType::LOAD_STOP);
  one->Show();

  ui_test_utils::RunAllPendingInMessageLoop();

  one->GetRootView()->RemoveChildView(dom_view);
  one->Hide();

  // Re-attach to another Widget.
  Widget* two = CreatePopupWindow();
  two->GetRootView()->AddChildView(dom_view);
  two->Show();

  ui_test_utils::RunAllPendingInMessageLoop();

  two->Hide();
}
