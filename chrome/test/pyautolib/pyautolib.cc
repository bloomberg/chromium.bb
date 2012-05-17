// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
#include "googleurl/src/gurl.h"

static int64 StringToId(const std::wstring& str) {
  int64 id;
  base::StringToInt64(WideToUTF8(str), &id);
  return id;
}

// PyUITestSuiteBase
PyUITestSuiteBase::PyUITestSuiteBase(int argc, char** argv)
    : UITestSuite(argc, argv) {
}

PyUITestSuiteBase::~PyUITestSuiteBase() {
#if defined(OS_MACOSX)
  pool_.Recycle();
#endif
  Shutdown();
}

void PyUITestSuiteBase::InitializeWithPath(const FilePath& browser_dir) {
  SetBrowserDirectory(browser_dir);
  UITestSuite::Initialize();
}

void PyUITestSuiteBase::SetCrSourceRoot(const FilePath& path) {
  PathService::Override(base::DIR_SOURCE_ROOT, path);
}

// PyUITestBase
PyUITestBase::PyUITestBase(bool clear_profile, std::wstring homepage)
    : UITestBase() {
  set_clear_profile(clear_profile);
  set_homepage(WideToASCII(homepage));
  // We add this so that pyauto can execute javascript in the renderer and
  // read values back.
  dom_automation_enabled_ = true;
  message_loop_ = GetSharedMessageLoop(MessageLoop::TYPE_DEFAULT);
}

PyUITestBase::~PyUITestBase() {
}

// static, refer .h for why it needs to be static
MessageLoop* PyUITestBase::message_loop_ = NULL;

// static
MessageLoop* PyUITestBase::GetSharedMessageLoop(
  MessageLoop::Type msg_loop_type) {
  if (!message_loop_)   // Create a shared instance of MessageLoop
    message_loop_ = new MessageLoop(msg_loop_type);
  return message_loop_;
}

void PyUITestBase::Initialize(const FilePath& browser_dir) {
  UITestBase::SetBrowserDirectory(browser_dir);
}

ProxyLauncher* PyUITestBase::CreateProxyLauncher() {
  if (named_channel_id_.empty())
    return new AnonymousProxyLauncher(false);
  return new NamedProxyLauncher(named_channel_id_, false, false);
}

void PyUITestBase::SetUp() {
  UITestBase::SetUp();
}

void PyUITestBase::TearDown() {
  UITestBase::TearDown();
}

void PyUITestBase::SetLaunchSwitches() {
  // Clear the homepage because some of the pyauto tests don't work correctly
  // if a URL argument is passed.
  std::string homepage_original;
  std::swap(homepage_original, homepage_);

  UITestBase::SetLaunchSwitches();

  // However, we *do* want the --homepage switch.
  std::swap(homepage_original, homepage_);
  launch_arguments_.AppendSwitchASCII(switches::kHomePage, homepage_);

}

void PyUITestBase::NavigateToURL(const char* url_string) {
  GURL url(url_string);
  UITestBase::NavigateToURL(url);
}

void PyUITestBase::NavigateToURL(const char* url_string, int window_index) {
  GURL url(url_string);
  UITestBase::NavigateToURL(url, window_index);
}

void PyUITestBase::NavigateToURL(
    const char* url_string, int window_index, int tab_index) {
  GURL url(url_string);
  UITestBase::NavigateToURL(url, window_index, tab_index);
}

void PyUITestBase::ReloadActiveTab(int window_index) {
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab_proxy->Reload());
}

bool PyUITestBase::AppendTab(const GURL& tab_url, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->AppendTab(tab_url);
}

bool PyUITestBase::ApplyAccelerator(int id, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->ApplyAccelerator(id);
}

bool PyUITestBase::RunCommand(int browser_command, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  return browser_proxy->RunCommand(browser_command);
}

bool PyUITestBase::IsMenuCommandEnabled(int id, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  bool enabled = false;
  if (browser_proxy.get())
    EXPECT_TRUE(browser_proxy->IsMenuCommandEnabled(id, &enabled));
  return enabled;
}

bool PyUITestBase::ActivateTab(int tab_index, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->BringToFront() && browser_proxy->ActivateTab(tab_index);
}

void PyUITestBase::SetDownloadShelfVisible(bool is_visible, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->SetShelfVisible(is_visible));
}

bool PyUITestBase::IsDownloadShelfVisible(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool visible = false;
  EXPECT_TRUE(browser_proxy->IsShelfVisible(&visible));
  return visible;
}

int PyUITestBase::GetTabCount(int window_index) {
  return UITestBase::GetTabCount(window_index);
}

GURL PyUITestBase::GetActiveTabURL(int window_index) {
  return UITestBase::GetActiveTabURL(window_index);
}

void PyUITestBase::OpenFindInPage(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->OpenFindInPage());
}

bool PyUITestBase::IsFindInPageVisible(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool is_visible;
  EXPECT_TRUE(browser_proxy->IsFindWindowFullyVisible(&is_visible));
  return is_visible;
}

FilePath PyUITestBase::GetDownloadDirectory() {
  FilePath download_dir;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return download_dir;
  EXPECT_TRUE(tab_proxy->GetDownloadDirectory(&download_dir));
  return download_dir;
}

bool PyUITestBase::OpenNewBrowserWindow(bool show) {
  return automation()->OpenNewBrowserWindow(Browser::TYPE_TABBED, show);
}

bool PyUITestBase::CloseBrowserWindow(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  if (!browser_proxy.get())
    return false;
  bool app_closed;
  return CloseBrowser(browser_proxy.get(), &app_closed);
}

int PyUITestBase::GetBrowserWindowCount() {
  int num_windows = 0;
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&num_windows));
  return num_windows;
}

bool PyUITestBase::GetBookmarkBarState(bool* visible, bool* detached) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  // We have no use for animating in this context.
  bool animating;
  EXPECT_TRUE(browser_proxy->GetBookmarkBarVisibility(visible,
                                                      &animating,
                                                      detached));
  return true;
}

bool PyUITestBase::GetBookmarkBarVisibility() {
  // We have no use for detached in this context.
  bool visible, detached;
  if (!GetBookmarkBarState(&visible, &detached))
    return false;
  return visible;
}

bool PyUITestBase::IsBookmarkBarDetached() {
  // We have no use for visible in this context.
  bool visible, detached;
  if (!GetBookmarkBarState(&visible, &detached))
    return false;
  return detached;
}

bool PyUITestBase::WaitForBookmarkBarVisibilityChange(bool wait_for_open) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  // This has a 20sec timeout.  If that's not enough we have serious problems.
  bool completed = UITestBase::WaitForBookmarkBarVisibilityChange(
      browser_proxy.get(),
      wait_for_open);
  EXPECT_TRUE(completed);
  return completed;
}

std::string PyUITestBase::_GetBookmarksAsJSON() {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return NULL;

  std::string s;
  EXPECT_TRUE(browser_proxy->GetBookmarksAsJSON(&s));
  return s;
}

bool PyUITestBase::AddBookmarkGroup(std::wstring& parent_id, int index,
                                    std::wstring& title) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->AddBookmarkGroup(StringToId(parent_id), index, title);
}

bool PyUITestBase::AddBookmarkURL(std::wstring& parent_id, int index,
                                  std::wstring& title, std::wstring& url) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->AddBookmarkURL(StringToId(parent_id),
                                       index, title,
                                       GURL(WideToUTF8(url)));
}

bool PyUITestBase::ReparentBookmark(
    std::wstring& id, std::wstring& new_parent_id, int index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->ReparentBookmark(StringToId(id),
                                         StringToId(new_parent_id),
                                         index);
}

bool PyUITestBase::SetBookmarkTitle(std::wstring& id, std::wstring& title) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->SetBookmarkTitle(StringToId(id), title);
}

bool PyUITestBase::SetBookmarkURL(std::wstring& id, std::wstring& url) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->SetBookmarkURL(StringToId(id), GURL(WideToUTF8(url)));
}

bool PyUITestBase::RemoveBookmark(std::wstring& id) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->RemoveBookmark(StringToId(id));
}

scoped_refptr<BrowserProxy> PyUITestBase::GetBrowserWindow(int window_index) {
  return automation()->GetBrowserWindow(window_index);
}

std::string PyUITestBase::_SendJSONRequest(int window_index,
                                           const std::string& request,
                                           int timeout) {
  std::string response;
  base::TimeTicks time;
  if (window_index < 0) {  // Do not need to target a browser window.
    time = base::TimeTicks::Now();
    if (!automation()->SendJSONRequest(request, timeout, &response)) {
        LOG(WARNING) << "SendJSONRequest returned false after "
                     << (base::TimeTicks::Now() - time).InSeconds()
                     << " seconds: " << request;
    }
  } else {
    scoped_refptr<BrowserProxy> browser_proxy =
        automation()->GetBrowserWindow(window_index);
    EXPECT_TRUE(browser_proxy.get());
    if (browser_proxy.get()) {
      time = base::TimeTicks::Now();
      if (!browser_proxy->SendJSONRequest(request, timeout, &response)) {
        LOG(WARNING) << "SendJSONRequest returned false after "
                     << (base::TimeTicks::Now() - time).InSeconds()
                     << " seconds: " << request;
      }
    }
  }
  return response;
}

bool PyUITestBase::ResetToDefaultTheme() {
  return automation()->ResetToDefaultTheme();
}

bool PyUITestBase::SetCookie(const GURL& cookie_url,
                             const std::string& value,
                             int window_index,
                             int tab_index) {
  scoped_refptr<BrowserProxy> browser_proxy = GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  scoped_refptr<TabProxy> tab_proxy = browser_proxy->GetTab(tab_index);
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return false;
  return tab_proxy->SetCookie(cookie_url, value);
}

std::string PyUITestBase::GetCookie(const GURL& cookie_url,
                                    int window_index,
                                    int tab_index) {
  std::string cookie_val;
  scoped_refptr<BrowserProxy> browser_proxy = GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  // TODO(phadjan.jr): figure out a way to unambiguously report error.
  if (!browser_proxy.get())
    return cookie_val;
  scoped_refptr<TabProxy> tab_proxy = browser_proxy->GetTab(tab_index);
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return cookie_val;
  EXPECT_TRUE(tab_proxy->GetCookies(cookie_url, &cookie_val));
  return cookie_val;
}
