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

std::wstring PyUITestSuite::GetActiveTabTitle() {
  std::wstring title;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return title;
  EXPECT_TRUE(tab_proxy->GetTabTitle(&title));
  return title;
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

