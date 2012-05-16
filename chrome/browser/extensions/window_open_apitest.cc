// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "net/base/mock_host_resolver.h"

using content::OpenURLParams;
using content::Referrer;

// Disabled, http://crbug.com/64899.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WindowOpen) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("window_open").AppendASCII("spanning")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

void WaitForTabsAndPopups(Browser* browser,
                          int num_tabs,
                          int num_popups,
                          int num_panels) {
  SCOPED_TRACE(
      StringPrintf("WaitForTabsAndPopups tabs:%d, popups:%d, panels:%d",
                   num_tabs, num_popups, num_panels));
  // We start with one tab and one browser already open.
  ++num_tabs;
  size_t num_browsers = static_cast<size_t>(num_popups + num_panels) + 1;

  const base::TimeDelta kWaitTime = base::TimeDelta::FromSeconds(15);
  base::TimeTicks end_time = base::TimeTicks::Now() + kWaitTime;
  while (base::TimeTicks::Now() < end_time) {
    if (browser::GetBrowserCount(browser->profile()) == num_browsers &&
        browser->tab_count() == num_tabs)
      break;

    ui_test_utils::RunAllPendingInMessageLoop();
  }

  EXPECT_EQ(num_browsers, browser::GetBrowserCount(browser->profile()));
  EXPECT_EQ(num_tabs, browser->tab_count());

  int num_popups_seen = 0;
  int num_panels_seen = 0;
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    if (*iter == browser)
      continue;

    // Check for TYPE_POPUP or TYPE_PANEL.
    EXPECT_TRUE((*iter)->is_type_popup() || (*iter)->is_type_panel());
    if ((*iter)->is_type_popup())
      ++num_popups_seen;
    else
      ++num_panels_seen;
  }
  EXPECT_EQ(num_popups, num_popups_seen);
  EXPECT_EQ(num_panels, num_panels_seen);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BrowserIsApp) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("browser_is_app")));

  WaitForTabsAndPopups(browser(), 0, 2, 0);

  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end(); ++iter) {
    if (*iter == browser())
      ASSERT_FALSE((*iter)->is_app());
    else
      ASSERT_TRUE((*iter)->is_app());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupDefault) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup")));

  const int num_tabs = 1;
  const int num_popups = 0;
  WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupLarge) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_large")));

  // On other systems this should open a new popup window.
  const int num_tabs = 0;
  const int num_popups = 1;
  WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupSmall) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_small")));

  // On ChromeOS this should open a new panel (acts like a new popup window).
  // On other systems this should open a new popup window.
  const int num_tabs = 0;
  const int num_popups = 1;
  WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PopupBlockingExtension) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("extension")));

  WaitForTabsAndPopups(browser(), 5, 3, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, PopupBlockingHostedApp) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(test_server()->Start());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("hosted_app")));

  // The app being tested owns the domain a.com .  The test URLs we navigate
  // to below must be within that domain, so that they fall within the app's
  // web extent.
  GURL::Replacements replace_host;
  std::string a_dot_com = "a.com";
  replace_host.SetHostStr(a_dot_com);

  const std::string popup_app_contents_path(
    "files/extensions/api_test/window_open/popup_blocking/hosted_app/");

  GURL open_tab =
      test_server()->GetURL(popup_app_contents_path + "open_tab.html")
          .ReplaceComponents(replace_host);
  GURL open_popup =
      test_server()->GetURL(popup_app_contents_path + "open_popup.html")
          .ReplaceComponents(replace_host);

  browser()->OpenURL(OpenURLParams(
      open_tab, Referrer(), NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_TYPED,
      false));
  browser()->OpenURL(OpenURLParams(
      open_popup, Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_TYPED, false));

  WaitForTabsAndPopups(browser(), 3, 1, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowArgumentsOverflow) {
  ASSERT_TRUE(RunExtensionTest("window_open/argument_overflow")) << message_;
}

class WindowOpenPanelDisabledTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    // TODO(jennb): Re-enable when panels are enabled by default.
    // command_line->AppendSwitch(switches::kDisablePanels);
  }
};

IN_PROC_BROWSER_TEST_F(WindowOpenPanelDisabledTest,
                       DISABLED_WindowOpenPanelNotEnabled) {
  ASSERT_TRUE(RunExtensionTest("window_open/panel_not_enabled")) << message_;
}

class WindowOpenPanelTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

#if defined(USE_AURA)
// On Aura, this currently fails because we're currently opening new panel
// windows as popup windows instead.
#define MAYBE_WindowOpenPanel FAILS_WindowOpenPanel
#else
#define MAYBE_WindowOpenPanel WindowOpenPanel
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, MAYBE_WindowOpenPanel) {
  ASSERT_TRUE(RunExtensionTest("window_open/panel")) << message_;
}

#if defined(OS_MACOSX) || defined(OS_WIN)
// Focus test fails if there is no window manager on Linux.
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, WindowOpenFocus) {
  ASSERT_TRUE(RunExtensionTest("window_open/focus")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest,
                       CloseNonExtensionPanelsOnUninstall) {
  ASSERT_TRUE(StartTestServer());

  // Setup listeners to wait on strings we expect the extension pages to send.
  std::vector<std::string> test_strings;
  test_strings.push_back("content_tab");
  test_strings.push_back("content_panel");
  test_strings.push_back("content_popup");

  ScopedVector<ExtensionTestMessageListener> listeners;
  for (size_t i = 0; i < test_strings.size(); ++i) {
    listeners.push_back(
        new ExtensionTestMessageListener(test_strings[i], false));
  }

  const Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII(
          "close_panels_on_uninstall"));
  ASSERT_TRUE(extension);

  // Two tabs. One in extension domain and one in non-extension domain.
  // Two popups - one in extension domain and one in non-extension domain.
  // Two panels - one in extension domain and one in non-extension domain.
  WaitForTabsAndPopups(browser(), 2, 2, 2);

  // Wait on test messages to make sure the pages loaded.
  for (size_t i = 0; i < listeners.size(); ++i)
    ASSERT_TRUE(listeners[i]->WaitUntilSatisfied());

  UninstallExtension(extension->id());

  // Wait for one tab and one popup in non-extension domain to stay open.
  // Expect everything else, including panels, to close.
  WaitForTabsAndPopups(browser(), 1, 1, 0);
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WindowOpener) {
  ASSERT_TRUE(RunExtensionTest("window_open/opener")) << message_;
}
