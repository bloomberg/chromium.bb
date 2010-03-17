// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
#include "googleurl/src/gurl.h"

PyUITestSuite::PyUITestSuite(
    int argc, char** argv, bool clear_profile, std::wstring homepage)
    : UITestSuite(argc, argv),
      UITestBase() {
  set_clear_profile(clear_profile);
  set_homepage(homepage);
}

PyUITestSuite::~PyUITestSuite() {
  UITestSuite::Shutdown();
}

void PyUITestSuite::Initialize(const FilePath& browser_dir) {
  UITestSuite::SetBrowserDirectory(browser_dir);
  UITestBase::SetBrowserDirectory(browser_dir);
  UITestSuite::Initialize();
}

void PyUITestSuite::SetUp() {
  UITestBase::SetUp();
}

void PyUITestSuite::TearDown() {
  UITestBase::TearDown();
}

void PyUITestSuite::NavigateToURL(const char* url_string) {
  GURL url(url_string);
  UITestBase::NavigateToURL(url);
}

void PyUITestSuite::NavigateToURL(
    const char* url_string, int window_index, int tab_index) {
  GURL url(url_string);
  UITestBase::NavigateToURL(url, window_index, tab_index);
}

bool PyUITestSuite::AppendTab(const GURL& tab_url, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->AppendTab(tab_url);
}

bool PyUITestSuite::ApplyAccelerator(int id, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->ApplyAccelerator(id);
}

bool PyUITestSuite::RunCommand(int browser_command, int window_index){
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  return browser_proxy->RunCommand(browser_command);
}

bool PyUITestSuite::ActivateTab(int tab_index, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  return browser_proxy->ActivateTab(tab_index);
}

void PyUITestSuite::SetDownloadShelfVisible(bool is_visible, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->SetShelfVisible(is_visible));
}

bool PyUITestSuite::IsDownloadShelfVisible(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool visible = false;
  EXPECT_TRUE(browser_proxy->IsShelfVisible(&visible));
  return visible;
}

int PyUITestSuite::GetTabCount(int window_index) {
  return UITestBase::GetTabCount(window_index);
}

GURL PyUITestSuite::GetActiveTabURL(int window_index) {
  return UITestBase::GetActiveTabURL(window_index);
}

void PyUITestSuite::OpenFindInPage(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->OpenFindInPage());
}

bool PyUITestSuite::IsFindInPageVisible(int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(window_index);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool is_visible;
  EXPECT_TRUE(browser_proxy->IsFindWindowFullyVisible(&is_visible));
  return is_visible;
}

FilePath PyUITestSuite::GetDownloadDirectory() {
  FilePath download_dir;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return download_dir;
  EXPECT_TRUE(tab_proxy->GetDownloadDirectory(&download_dir));
  return download_dir;
}

bool PyUITestSuite::OpenNewBrowserWindow(bool show) {
  return automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, show);
}

bool PyUITestSuite::InstallExtension(const FilePath& crx_file) {
  return automation()->InstallExtension(crx_file);
}

bool PyUITestSuite::GetBookmarkBarVisibility() {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  // We have no use for animating in this context.
  bool visible, animating;
  EXPECT_TRUE(browser_proxy->GetBookmarkBarVisibility(&visible, &animating));
  return visible;
}

bool PyUITestSuite::WaitForBookmarkBarVisibilityChange(bool wait_for_open) {
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

std::string PyUITestSuite::_GetBookmarksAsJSON() {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  std::string s;
  EXPECT_TRUE(browser_proxy->GetBookmarksAsJSON(&s));
  return s;
}

bool PyUITestSuite::AddBookmarkGroup(std::wstring& parent_id, int index,
                                     std::wstring& title) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->AddBookmarkGroup(StringToInt64(WideToUTF16(parent_id)),
                                         index, title);
}

bool PyUITestSuite::AddBookmarkURL(std::wstring& parent_id, int index,
                                   std::wstring& title, std::wstring& url) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->AddBookmarkURL(StringToInt64(WideToUTF16(parent_id)),
                                       index, title,
                                       GURL(WideToUTF16(url)));
}

bool PyUITestSuite::ReparentBookmark(std::wstring& id, std::wstring& new_parent_id,
                                     int index) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->ReparentBookmark(
      StringToInt64(WideToUTF16(id)),
      StringToInt64(WideToUTF16(new_parent_id)),
      index);
}

bool PyUITestSuite::SetBookmarkTitle(std::wstring& id, std::wstring& title) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->SetBookmarkTitle(StringToInt64(WideToUTF16(id)),
                                         title);
}

bool PyUITestSuite::SetBookmarkURL(std::wstring& id, std::wstring& url) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->SetBookmarkURL(StringToInt64(WideToUTF16(id)),
                                       GURL(WideToUTF16(url)));
}

bool PyUITestSuite::RemoveBookmark(std::wstring& id) {
  scoped_refptr<BrowserProxy> browser_proxy =
      automation()->GetBrowserWindow(0);  // Window doesn't matter.
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;

  return browser_proxy->RemoveBookmark(StringToInt64(WideToUTF16(id)));
}


