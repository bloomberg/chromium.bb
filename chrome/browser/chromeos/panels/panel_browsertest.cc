// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"

namespace chromeos {

class PanelTest : public InProcessBrowserTest {
 public:
  PanelTest() {
    EnableDOMAutomation();
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }
};

// Small popups should open as a panel.
IN_PROC_BROWSER_TEST_F(PanelTest, PanelOpenSmall) {
  const std::string HTML =
      "<html><head><title>PanelOpen</title></head>"
      "<body onload='window.setTimeout(run_tests, 0)'>"
      "<script>"
      "  function run_tests() {"
      "    window.open(null, null, 'width=100,height=100');"
      "  }"
      "</script>"
      "</body></html>";
  GURL url("data:text/html," + HTML);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  // Wait for notification that window.open has been processed.
  ui_test_utils::WaitForNotification(NotificationType::TAB_ADDED);

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }

  ASSERT_TRUE(new_browser);
  EXPECT_EQ(Browser::TYPE_POPUP, new_browser->type());
  // This window type tells the cros window manager to treat the window
  // as a panel.
  EXPECT_EQ(
      WM_IPC_WINDOW_CHROME_PANEL_CONTENT,
      WmIpc::instance()->GetWindowType(
          GTK_WIDGET(new_browser->window()->GetNativeHandle()), NULL));
}

// Large popups should open as new tab.
IN_PROC_BROWSER_TEST_F(PanelTest, PanelOpenLarge) {
  const std::string HTML =
      "<html><head><title>PanelOpen</title></head>"
      "<body onload='window.setTimeout(run_tests, 0)'>"
      "<script>"
      "  function run_tests() {"
      "    window.open(null, null, 'width=1000,height=1000');"
      "  }"
        "</script>"
      "</body></html>";
  GURL url("data:text/html," + HTML);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);
  int old_tab_count = browser()->tab_count();
  browser()->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);

  // Wait for notification that window.open has been processed.
  ui_test_utils::WaitForNotification(NotificationType::TAB_ADDED);

  // Shouldn't find a new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  EXPECT_FALSE(new_browser);

  // Should find a new tab.
  EXPECT_EQ(old_tab_count + 1, browser()->tab_count());
}

}  // namespace chromeos
