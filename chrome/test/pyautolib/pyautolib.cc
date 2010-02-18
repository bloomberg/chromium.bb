// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
#include "googleurl/src/gurl.h"

PyUITestSuite::PyUITestSuite(int argc, char** argv)
    : UITestSuite(argc, argv),
      UITestBase() {
  UITestSuite::Initialize();
}

PyUITestSuite::~PyUITestSuite() {
  UITestSuite::Shutdown();
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

bool PyUITestSuite::ActivateTab(int tab_index, int window_index) {
  scoped_refptr<BrowserProxy> browser_proxy =
    automation()->GetBrowserWindow(window_index);
  return browser_proxy->ActivateTab(tab_index);
}

void PyUITestSuite::SetShelfVisible(bool is_visible) {
  scoped_refptr<BrowserProxy> browser_proxy = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->SetShelfVisible(is_visible));
}

bool PyUITestSuite::IsShelfVisible() {
  scoped_refptr<BrowserProxy> browser_proxy = automation()->GetBrowserWindow(0);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool visible = false;
  EXPECT_TRUE(browser_proxy->IsShelfVisible(&visible));
  return visible;
}

GURL PyUITestSuite::GetActiveTabURL() {
  return UITestBase::GetActiveTabURL();
}

void PyUITestSuite::OpenFindInPage() {
  scoped_refptr<BrowserProxy> browser_proxy = automation()->GetBrowserWindow(0);
  ASSERT_TRUE(browser_proxy.get());
  EXPECT_TRUE(browser_proxy->OpenFindInPage());
}

bool PyUITestSuite::IsFindInPageVisible() {
  scoped_refptr<BrowserProxy> browser_proxy = automation()->GetBrowserWindow(0);
  EXPECT_TRUE(browser_proxy.get());
  if (!browser_proxy.get())
    return false;
  bool is_visible;
  EXPECT_TRUE(browser_proxy->IsFindWindowFullyVisible(&is_visible));
  return is_visible;
}

std::string PyUITestSuite::GetDownloadDirectory() {
  FilePath download_dir;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return download_dir.value();
  EXPECT_TRUE(tab_proxy->GetDownloadDirectory(&download_dir));
  return download_dir.value();
}

bool PyUITestSuite::OpenNewBrowserWindow(bool show) {
  return automation()->OpenNewBrowserWindow(Browser::TYPE_NORMAL, show);
}

bool PyUITestSuite::InstallExtension(const FilePath& crx_file) {
  return automation()->InstallExtension(crx_file);
}

