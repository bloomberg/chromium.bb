// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_vector.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_ASH)
#include "chrome/browser/extensions/shell_window_registry.h"
#endif

#if defined(USE_ASH) && !defined(OS_WIN)
// TODO(stevenjb): Figure out the correct behavior for Ash + Win
#define USE_ASH_PANELS
#endif

using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

// Disabled, http://crbug.com/64899.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WindowOpen) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ResultCatcher catcher;
  ASSERT_TRUE(LoadExtensionIncognito(test_data_dir_
      .AppendASCII("window_open").AppendASCII("spanning")));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

int GetPanelCount(Browser* browser) {
#if defined(USE_ASH_PANELS)
  return static_cast<int>(extensions::ShellWindowRegistry::Get(
      browser->profile())->shell_windows().size());
#else
  return PanelManager::GetInstance()->num_panels();
#endif
}

bool WaitForTabsAndPopups(Browser* browser,
                          int num_tabs,
                          int num_popups,
                          int num_panels) {
  SCOPED_TRACE(
      StringPrintf("WaitForTabsAndPopups tabs:%d, popups:%d, panels:%d",
                   num_tabs, num_popups, num_panels));
  // We start with one tab and one browser already open.
  ++num_tabs;
  size_t num_browsers = static_cast<size_t>(num_popups) + 1;

  const base::TimeDelta kWaitTime = base::TimeDelta::FromSeconds(10);
  base::TimeTicks end_time = base::TimeTicks::Now() + kWaitTime;
  while (base::TimeTicks::Now() < end_time) {
    if (chrome::GetBrowserCount(browser->profile(),
                                browser->host_desktop_type()) == num_browsers &&
        browser->tab_strip_model()->count() == num_tabs &&
        GetPanelCount(browser) == num_panels)
      break;

    content::RunAllPendingInMessageLoop();
  }

  EXPECT_EQ(num_browsers,
            chrome::GetBrowserCount(browser->profile(),
                                    browser->host_desktop_type()));
  EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
  EXPECT_EQ(num_panels, GetPanelCount(browser));

  int num_popups_seen = 0;
  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    if (*iter == browser)
      continue;

    // Check for TYPE_POPUP.
#if defined(USE_ASH_PANELS)
    // On Ash, panel windows may open as popup windows.
    EXPECT_TRUE((*iter)->is_type_popup() || (*iter)->is_type_panel());
#else
    EXPECT_TRUE((*iter)->is_type_popup());
#endif
    ++num_popups_seen;
  }
  EXPECT_EQ(num_popups, num_popups_seen);

  return ((num_browsers ==
               chrome::GetBrowserCount(browser->profile(),
                                       browser->host_desktop_type())) &&
          (num_tabs == browser->tab_strip_model()->count()) &&
          (num_panels == GetPanelCount(browser)) &&
          (num_popups == num_popups_seen));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, BrowserIsApp) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("browser_is_app")));

  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 0, 2, 0));

  for (chrome::BrowserIterator iter; !iter.done(); iter.Next()) {
    if (*iter == browser())
      ASSERT_FALSE(iter->is_app());
    else
      ASSERT_TRUE(iter->is_app());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupDefault) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup")));

  const int num_tabs = 1;
  const int num_popups = 0;
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupLarge) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_large")));

  // On other systems this should open a new popup window.
  const int num_tabs = 0;
  const int num_popups = 1;
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowOpenPopupSmall) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_small")));

  // On ChromeOS this should open a new panel (acts like a new popup window).
  // On other systems this should open a new popup window.
  const int num_tabs = 0;
  const int num_popups = 1;
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), num_tabs, num_popups, 0));
}

// Disabled on Windows. Often times out or fails: crbug.com/177530
#if defined(OS_WIN)
#define MAYBE_PopupBlockingExtension DISABLED_PopupBlockingExtension
#else
#define MAYBE_PopupBlockingExtension PopupBlockingExtension
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_PopupBlockingExtension) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII("popup_blocking")
      .AppendASCII("extension")));

  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 5, 3, 0));
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

  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 3, 1, 0));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WindowArgumentsOverflow) {
  ASSERT_TRUE(RunExtensionTest("window_open/argument_overflow")) << message_;
}

class WindowOpenPanelDisabledTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnablePanels);
  }
};

#if defined(USE_ASH_PANELS)
// On Ash, this currently fails because we're currently opening new panel
// windows as popup windows instead.
#define MAYBE_WindowOpenPanel DISABLED_WindowOpenPanel
#else
#define MAYBE_WindowOpenPanel WindowOpenPanel
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, MAYBE_WindowOpenPanel) {
  ASSERT_TRUE(RunExtensionTest("window_open/panel")) << message_;
}

#if defined(USE_ASH_PANELS) || defined(OS_WIN)
// On Ash, this currently fails because we're currently opening new panel
// windows as popup windows instead.
// This is also flakey on Windows builds: crbug.com/179251
#define MAYBE_WindowOpenPanelDetached DISABLED_WindowOpenPanelDetached
#else
#define MAYBE_WindowOpenPanelDetached WindowOpenPanelDetached
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, MAYBE_WindowOpenPanelDetached) {
  ASSERT_TRUE(RunExtensionTest("window_open/panel_detached")) << message_;
}

IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest,
                       CloseNonExtensionPanelsOnUninstall) {
#if defined(USE_ASH_PANELS)
  // On Ash, new panel windows open as popup windows instead.
  int num_popups, num_panels;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePanels)) {
    num_popups = 2;
    num_panels = 2;
  } else {
    num_popups = 4;
    num_panels = 0;
  }
#else
  int num_popups = 2;
  int num_panels = 2;
#endif
  ASSERT_TRUE(StartTestServer());

  // Setup listeners to wait on strings we expect the extension pages to send.
  std::vector<std::string> test_strings;
  test_strings.push_back("content_tab");
  if (num_panels)
    test_strings.push_back("content_panel");
  test_strings.push_back("content_popup");

  ScopedVector<ExtensionTestMessageListener> listeners;
  for (size_t i = 0; i < test_strings.size(); ++i) {
    listeners.push_back(
        new ExtensionTestMessageListener(test_strings[i], false));
  }

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII(
          "close_panels_on_uninstall"));
  ASSERT_TRUE(extension);

  // Two tabs. One in extension domain and one in non-extension domain.
  // Two popups - one in extension domain and one in non-extension domain.
  // Two panels - one in extension domain and one in non-extension domain.
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 2, num_popups, num_panels));

  // Wait on test messages to make sure the pages loaded.
  for (size_t i = 0; i < listeners.size(); ++i)
    ASSERT_TRUE(listeners[i]->WaitUntilSatisfied());

  UninstallExtension(extension->id());

  // Wait for the tabs and popups in non-extension domain to stay open.
  // Expect everything else, including panels, to close.
  num_popups -= 1;
#if defined(USE_ASH_PANELS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnablePanels)) {
    // On Ash, new panel windows open as popup windows instead, so there are 2
    // extension domain popups that will close (instead of 1 popup on non-Ash).
    num_popups -= 1;
  }
#endif
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 1, num_popups, 0));
}

#if defined(OS_CHROMEOS)
// TODO(derat): See if there's some way to get this to work on Chrome OS.  It
// crashes there, apparently because we automatically reload crashed pages:
// http:/crbug.com/161073
#define MAYBE_ClosePanelsOnExtensionCrash DISABLED_ClosePanelsOnExtensionCrash
#else
#define MAYBE_ClosePanelsOnExtensionCrash ClosePanelsOnExtensionCrash
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, MAYBE_ClosePanelsOnExtensionCrash) {
#if defined(USE_ASH_PANELS)
  // On Ash, new panel windows open as popup windows instead.
  int num_popups = 4;
  int num_panels = 0;
#else
  int num_popups = 2;
  int num_panels = 2;
#endif
  ASSERT_TRUE(StartTestServer());

  // Setup listeners to wait on strings we expect the extension pages to send.
  std::vector<std::string> test_strings;
  test_strings.push_back("content_tab");
  if (num_panels)
    test_strings.push_back("content_panel");
  test_strings.push_back("content_popup");

  ScopedVector<ExtensionTestMessageListener> listeners;
  for (size_t i = 0; i < test_strings.size(); ++i) {
    listeners.push_back(
        new ExtensionTestMessageListener(test_strings[i], false));
  }

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("window_open").AppendASCII(
          "close_panels_on_uninstall"));
  ASSERT_TRUE(extension);

  // Two tabs. One in extension domain and one in non-extension domain.
  // Two popups - one in extension domain and one in non-extension domain.
  // Two panels - one in extension domain and one in non-extension domain.
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 2, num_popups, num_panels));

  // Wait on test messages to make sure the pages loaded.
  for (size_t i = 0; i < listeners.size(); ++i)
    ASSERT_TRUE(listeners[i]->WaitUntilSatisfied());

  // Crash the extension.
  extensions::ExtensionHost* extension_host =
      extensions::ExtensionSystem::Get(browser()->profile())->
          process_manager()->GetBackgroundHostForExtension(extension->id());
  ASSERT_TRUE(extension_host);
  base::KillProcess(extension_host->render_process_host()->GetHandle(),
                    content::RESULT_CODE_KILLED, false);
  WaitForExtensionCrash(extension->id());

  // Only expect panels to close. The rest stay open to show a sad-tab.
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 2, num_popups, 0));
}

#if defined(USE_ASH_PANELS)
// This test is not applicable on Ash. The modified window.open behavior only
// applies to non-Ash panel windows.
#define MAYBE_WindowOpenFromPanel DISABLED_WindowOpenFromPanel
#else
#define MAYBE_WindowOpenFromPanel WindowOpenFromPanel
#endif
IN_PROC_BROWSER_TEST_F(WindowOpenPanelTest, MAYBE_WindowOpenFromPanel) {
  ASSERT_TRUE(StartTestServer());

  // Load the extension that will open a panel which then calls window.open.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("window_open").
                            AppendASCII("panel_window_open")));

  // Expect one panel (opened by extension) and one tab (from the panel calling
  // window.open). Panels modify the WindowOpenDisposition in window.open
  // to always open in a tab.
  EXPECT_TRUE(WaitForTabsAndPopups(browser(), 1, 0, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_WindowOpener) {
  ASSERT_TRUE(RunExtensionTest("window_open/opener")) << message_;
}

// Tests that an extension page can call window.open to an extension URL and
// the new window has extension privileges.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  GURL start_url(std::string("chrome-extension://") +
      last_loaded_extension_id_ + "/test.html");
  ui_test_utils::NavigateToURL(browser(), start_url);
  WebContents* newtab = NULL;
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(browser()->tab_strip_model()->GetActiveWebContents(),
      start_url.Resolve("newtab.html"), true, &newtab));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(newtab, "testExtensionApi()",
                                                   &result));
  EXPECT_TRUE(result);
}

// Tests that if an extension page calls window.open to an invalid extension
// URL, the browser doesn't crash.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenInvalidExtension) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  GURL start_url(std::string("chrome-extension://") +
      last_loaded_extension_id_ + "/test.html");
  ui_test_utils::NavigateToURL(browser(), start_url);
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(browser()->tab_strip_model()->GetActiveWebContents(),
      GURL("chrome-extension://thisissurelynotavalidextensionid/newtab.html"),
      false, NULL));

  // If we got to this point, we didn't crash, so we're good.
}

// Tests that calling window.open from the newtab page to an extension URL
// gives the new window extension privileges - even though the opening page
// does not have extension privileges, we break the script connection, so
// there is no privilege leak.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WindowOpenNoPrivileges) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("uitest").AppendASCII("window_open")));

  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WebContents* newtab = NULL;
  ASSERT_NO_FATAL_FAILURE(
      OpenWindow(browser()->tab_strip_model()->GetActiveWebContents(),
      GURL(std::string("chrome-extension://") + last_loaded_extension_id_ +
          "/newtab.html"), false, &newtab));

  // Extension API should succeed.
  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(newtab, "testExtensionApi()",
                                                   &result));
  EXPECT_TRUE(result);
}
