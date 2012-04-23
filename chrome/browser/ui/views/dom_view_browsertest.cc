// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/views/widget/widget.h"

using views::Widget;

class DOMViewTest : public InProcessBrowserTest {
 public:
  Widget* CreatePopupWindow() {
    Widget* widget = new Widget;
    Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(0, 0, 400, 400);
    widget->Init(params);
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
  ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  dom_view->LoadURL(GURL("http://www.google.com"));
  load_stop_observer.Wait();
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
  ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  dom_view->LoadURL(GURL("http://www.google.com"));
  load_stop_observer.Wait();
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
  ui_test_utils::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  dom_view->LoadURL(GURL("http://www.google.com"));
  load_stop_observer.Wait();
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
