// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

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

  ui_test_utils::WindowedNotificationObserver tab_added_observer(
      content::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  tab_added_observer.Wait();

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }

  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(new_browser->is_type_popup());
  EXPECT_FALSE(new_browser->is_app());

#if defined(TOOLKIT_USES_GTK)
  // This window type tells the cros window manager to treat the window
  // as a panel.
  EXPECT_EQ(
      WM_IPC_WINDOW_CHROME_PANEL_CONTENT,
      WmIpc::instance()->GetWindowType(
          GTK_WIDGET(new_browser->window()->GetNativeHandle()), NULL));
#endif
}

#if defined(USE_AURA)
// crbug.com/105129.
#define MAYBE_PanelOpenLarge DISABLED_PanelOpenLarge
#else
#define MAYBE_PanelOpenLarge PanelOpenLarge
#endif

// Large popups should open as new tab.
IN_PROC_BROWSER_TEST_F(PanelTest, MAYBE_PanelOpenLarge) {
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
  ui_test_utils::WindowedNotificationObserver tab_added_observer(
      content::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  browser()->OpenURL(url, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);
  tab_added_observer.Wait();

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
