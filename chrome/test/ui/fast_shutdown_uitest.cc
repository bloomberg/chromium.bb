// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/message_box_flags.h"
#include "base/gfx/rect.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/view_ids.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "views/event.h"

class FastShutdown : public UITest {
};

// This tests for a previous error where uninstalling an onbeforeunload
// handler would enable fast shutdown even if an onUnload handler still
// existed.
TEST_F(FastShutdown, SlowTermination) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // This page has an unload handler.
  GURL url = GetTestUrl(L"fast_shutdown", L"on_unloader.html");
  NavigateToURLBlockUntilNavigationsComplete(url, 1);
  gfx::Rect bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &bounds, true));
  // This click will launch a popup which has a before unload handler.
  ASSERT_TRUE(window->SimulateOSClick(bounds.CenterPoint(),
                                      views::Event::EF_LEFT_BUTTON_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2, action_timeout_ms()));
  // Close the tab, removing the one and only before unload handler.
  scoped_refptr<TabProxy> tab(browser->GetTab(1));
  ASSERT_TRUE(tab->Close(true));

  // Close the browser. We should launch the unload handler, which is an
  // alert().
  ASSERT_TRUE(browser->ApplyAccelerator(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->WaitForAppModalDialog(action_timeout_ms()));
  automation()->ClickAppModalDialogButton(MessageBoxFlags::DIALOGBUTTON_OK);
}
