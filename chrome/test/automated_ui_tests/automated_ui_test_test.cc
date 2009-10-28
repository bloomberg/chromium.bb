// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automated_ui_tests/automated_ui_test_base.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_MACOSX)
// The window pops up, but doesn't close.
#define MAYBE_IncognitoWindow DISABLED_IncognitoWindow
#define MAYBE_OpenCloseBrowserWindowWithAccelerator \
    DISABLED_OpenCloseBrowserWindowWithAccelerator
#else
// http://code.google.com/p/chromium/issues/detail?id=14731
#define MAYBE_IncognitoWindow DISABLED_IncognitoWindow
// http://crbug.com/19394
#define MAYBE_OpenCloseBrowserWindowWithAccelerator \
    DISABLED_OpenCloseBrowserWindowWithAccelerator
#endif

// http://code.google.com/p/chromium/issues/detail?id=14774
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_CloseTab DISABLED_CloseTab
#else
#define MAYBE_CloseTab CloseTab
#endif

// Automation pieces are not implemented for these on platforms other than
// Windows.
#if defined(OS_WIN)
#define MAYBE_ShowBookmarkBar ShowBookmarkBar
#else
#define MAYBE_ShowBookmarkBar DISABLED_ShowBookmarkBar
#endif

// FindBarTesting not implemented on mac.
#if defined(OS_MACOSX)
#define MAYBE_FindInPage DISABLED_FindInPage
#else
#define MAYBE_FindInPage FindInPage
#endif

TEST_F(AutomatedUITestBase, MAYBE_FindInPage) {
  ASSERT_TRUE(FindInPage());
  bool is_visible;
  ASSERT_TRUE(active_browser()->IsFindWindowFullyVisible(&is_visible));
  EXPECT_TRUE(is_visible);
}

TEST_F(AutomatedUITestBase, Home) {
  FilePath path_prefix(test_data_directory_.AppendASCII("session_history"));
  GURL bot1(net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html")));
  NavigateToURL(bot1);  // To help verify that Home does something.

  ASSERT_TRUE(Home());

  GURL url;
  ASSERT_TRUE(active_browser()->GetActiveTab()->GetCurrentURL(&url));
  EXPECT_EQ(GURL(chrome::kAboutBlankURL), url);

  std::wstring title;
  ASSERT_TRUE(active_browser()->GetActiveTab()->GetTabTitle(&title));
  EXPECT_EQ(L"", title);
}

TEST_F(AutomatedUITestBase, NewTab) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
}

TEST_F(AutomatedUITestBase, DuplicateTab) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  DuplicateTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  DuplicateTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
}

TEST_F(AutomatedUITestBase, DISABLED_RestoreTab) {
  int tab_count;
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  FilePath path_prefix(test_data_directory_.AppendASCII("session_history"));
  GURL test_url = net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html"));
  GetActiveTab()->NavigateToURL(test_url);
  CloseActiveTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  RestoreTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
}

TEST_F(AutomatedUITestBase, MAYBE_CloseTab) {
  int num_browser_windows;
  int tab_count;
  NewTab();
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);

  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(NULL));
  NewTab();
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(2, num_browser_windows);

  ASSERT_TRUE(CloseActiveTab());
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  ASSERT_TRUE(CloseActiveTab());
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  num_browser_windows = 0;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(2, num_browser_windows);

  // The browser window is closed by closing this tab.
  ASSERT_TRUE(CloseActiveTab());
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
  // Active_browser_ is now the first created window.
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  ASSERT_TRUE(CloseActiveTab());
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);

  // The last tab should not be closed.
  ASSERT_FALSE(CloseActiveTab());
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
}

TEST_F(AutomatedUITestBase, OpenBrowserWindow) {
  int num_browser_windows;
  int tab_count;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);

  scoped_refptr<BrowserProxy> browser_1;
  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(&browser_1));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(2, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  browser_1->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);

  scoped_refptr<BrowserProxy> browser_2;
  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(&browser_2));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(3, num_browser_windows);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  NewTab();
  NewTab();
  browser_1->GetTabCount(&tab_count);
  ASSERT_EQ(1, tab_count);
  browser_2->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);

  bool application_closed;
  CloseBrowser(browser_1.get(), &application_closed);
  ASSERT_FALSE(application_closed);
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(2, num_browser_windows);
  CloseBrowser(browser_2.get(), &application_closed);
  ASSERT_FALSE(application_closed);
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
}

// http://code.google.com/p/chromium/issues/detail?id=14774
TEST_F(AutomatedUITestBase, FLAKY_CloseBrowserWindow) {
  int tab_count;
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(2, tab_count);

  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(NULL));
  NewTab();
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(3, tab_count);

  ASSERT_TRUE(OpenAndActivateNewBrowserWindow(NULL));
  NewTab();
  NewTab();
  NewTab();
  active_browser()->GetTabCount(&tab_count);
  ASSERT_EQ(4, tab_count);

  ASSERT_TRUE(CloseActiveWindow());
  active_browser()->GetTabCount(&tab_count);

  if (tab_count == 2) {
    ASSERT_TRUE(CloseActiveWindow());
    active_browser()->GetTabCount(&tab_count);
    ASSERT_EQ(3, tab_count);
  } else {
    ASSERT_EQ(3, tab_count);
    ASSERT_TRUE(CloseActiveWindow());
    active_browser()->GetTabCount(&tab_count);
    ASSERT_EQ(2, tab_count);
  }

  ASSERT_FALSE(CloseActiveWindow());
}

TEST_F(AutomatedUITestBase, MAYBE_IncognitoWindow) {
  int num_browser_windows;
  int num_normal_browser_windows;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
  automation()->GetNormalBrowserWindowCount(&num_normal_browser_windows);
  ASSERT_EQ(1, num_normal_browser_windows);

  ASSERT_TRUE(GoOffTheRecord());
  ASSERT_TRUE(GoOffTheRecord());
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(3, num_browser_windows);
  automation()->GetNormalBrowserWindowCount(&num_normal_browser_windows);
  ASSERT_EQ(1, num_normal_browser_windows);

  // There is only one normal window so it will not be closed.
  ASSERT_FALSE(CloseActiveWindow());
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(3, num_browser_windows);
  automation()->GetNormalBrowserWindowCount(&num_normal_browser_windows);
  ASSERT_EQ(1, num_normal_browser_windows);

  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
}

TEST_F(AutomatedUITestBase, MAYBE_OpenCloseBrowserWindowWithAccelerator) {
  // Note: we don't use RunCommand(IDC_OPEN/CLOSE_WINDOW) to open/close
  // browser window in automated ui tests. Instead we use
  // OpenAndActivateNewBrowserWindow and CloseActiveWindow.
  // There are other parts of UI test that use the accelerators. This is
  // a unit test for those usage.
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  int num_browser_windows;
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(2, num_browser_windows);
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  ASSERT_TRUE(RunCommand(IDC_NEW_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(7, num_browser_windows);

  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(6, num_browser_windows);
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  set_active_browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(RunCommand(IDC_CLOSE_WINDOW));
  ASSERT_TRUE(automation()->GetBrowserWindowCount(&num_browser_windows));
  ASSERT_EQ(1, num_browser_windows);
}

TEST_F(AutomatedUITestBase, Navigate) {
  FilePath path_prefix(test_data_directory_.AppendASCII("session_history"));
  GURL url1(net::FilePathToFileURL(path_prefix.AppendASCII("bot1.html")));
  GURL url2(net::FilePathToFileURL(path_prefix.AppendASCII("bot2.html")));
  GURL url3(net::FilePathToFileURL(path_prefix.AppendASCII("bot3.html")));
  GURL url;
  ASSERT_TRUE(Navigate(url1));
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url1, url);
  ASSERT_TRUE(Navigate(url2));
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);
  ASSERT_TRUE(Navigate(url3));
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url3, url);
  ASSERT_TRUE(BackButton());
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);
  ASSERT_TRUE(BackButton());
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url1, url);
  ASSERT_TRUE(ForwardButton());
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);
  ASSERT_TRUE(ReloadPage());
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(url2, url);
}

TEST_F(AutomatedUITestBase, SelectTab) {
  FilePath filename(test_data_directory_);
  filename = filename.AppendASCII("title2.html");
  GURL url = net::FilePathToFileURL(filename);

  ASSERT_TRUE(active_browser()->AppendTab(url));
  ASSERT_TRUE(active_browser()->AppendTab(url));

  int active_tab_index;
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(2, active_tab_index);

  ASSERT_TRUE(SelectNextTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);

  ASSERT_TRUE(SelectNextTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  ASSERT_TRUE(SelectPreviousTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(0, active_tab_index);

  ASSERT_TRUE(SelectPreviousTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(2, active_tab_index);

  ASSERT_TRUE(SelectPreviousTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(1, active_tab_index);

  ASSERT_TRUE(SelectNextTab());
  ASSERT_TRUE(active_browser()->GetActiveTabIndex(&active_tab_index));
  ASSERT_EQ(2, active_tab_index);
}

TEST_F(AutomatedUITestBase, MAYBE_ShowBookmarkBar) {
  ASSERT_TRUE(ShowBookmarkBar());
  bool is_visible;
  bool is_animating;
  ASSERT_TRUE(active_browser()->GetBookmarkBarVisibility(&is_visible,
                                                         &is_animating));
  ASSERT_TRUE(is_visible);
  ASSERT_FALSE(is_animating);

  // Try second time to make sure it won't make the bookmark bar
  // disappear.
  ASSERT_TRUE(ShowBookmarkBar());
  ASSERT_TRUE(active_browser()->GetBookmarkBarVisibility(&is_visible,
                                                         &is_animating));
  ASSERT_TRUE(is_visible);
  ASSERT_FALSE(is_animating);
}

TEST_F(AutomatedUITestBase, ShowDownloads) {
  ASSERT_TRUE(ShowDownloads());
  GURL url;
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(GURL(chrome::kChromeUIDownloadsURL), url);
}

TEST_F(AutomatedUITestBase, ShowHistory) {
  ASSERT_TRUE(ShowHistory());
  GURL url;
  ASSERT_TRUE(GetActiveTab()->GetCurrentURL(&url));
  ASSERT_EQ(GURL(chrome::kChromeUIHistoryURL), url);
}
