// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome/test/ui_test_utils.h"
#include "ui/base/message_box_flags.h"
#include "ui/gfx/rect.h"
#include "views/event.h"

class FastShutdown : public UITest {
};

#if defined(OS_MACOSX)
// SimulateOSClick is broken on the Mac: http://crbug.com/45162
#define MAYBE_SlowTermination DISABLED_SlowTermination
#else
#define MAYBE_SlowTermination SlowTermination
#endif

// This tests for a previous error where uninstalling an onbeforeunload
// handler would enable fast shutdown even if an onUnload handler still
// existed.
TEST_F(FastShutdown, MAYBE_SlowTermination) {
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  scoped_refptr<WindowProxy> window(browser->GetWindow());
  ASSERT_TRUE(window.get());

  // This page has an unload handler.
  const FilePath dir(FILE_PATH_LITERAL("fast_shutdown"));
  const FilePath file(FILE_PATH_LITERAL("on_unloader.html"));
  NavigateToURL(ui_test_utils::GetTestUrl(dir, file));
  gfx::Rect bounds;
  ASSERT_TRUE(window->GetViewBounds(VIEW_ID_TAB_CONTAINER, &bounds, true));
  // This click will launch a popup which has a before unload handler.
  ASSERT_TRUE(window->SimulateOSClick(bounds.CenterPoint(),
                                      views::Event::EF_LEFT_BUTTON_DOWN));
  ASSERT_TRUE(browser->WaitForTabCountToBecome(2));
  // Close the tab, removing the one and only before unload handler.
  scoped_refptr<TabProxy> tab(browser->GetTab(1));
  ASSERT_TRUE(tab->Close(true));

  // Close the browser. We should launch the unload handler, which is an
  // alert().
  ASSERT_TRUE(browser->ApplyAccelerator(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->WaitForAppModalDialog());
  ASSERT_TRUE(automation()->ClickAppModalDialogButton(
                  ui::MessageBoxFlags::DIALOGBUTTON_OK));
  ASSERT_TRUE(WaitForBrowserProcessToQuit(
      TestTimeouts::wait_for_terminate_timeout_ms()));
}
