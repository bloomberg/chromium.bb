// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/test/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BrowserInitTest : public InProcessBrowserTest {
};

class OpenURLsPopupObserver : public BrowserList::Observer {
 public:
  OpenURLsPopupObserver() : added_browser_(NULL) { }

  virtual void OnBrowserAdded(const Browser* browser) {
    added_browser_ = browser;
  }

  virtual void OnBrowserRemoving(const Browser* browser) { }

  const Browser* added_browser_;
};

// Test that when there is a popup as the active browser any requests to
// BrowserInit::LaunchWithProfile::OpenURLsInBrowser don't crash because
// there's no explicit profile given.
IN_PROC_BROWSER_TEST_F(BrowserInitTest, OpenURLsPopup) {
  std::vector<GURL> urls;
  urls.push_back(GURL("http://localhost"));

  // Note that in our testing we do not ever query the BrowserList for the "last
  // active" browser. That's because the browsers are set as "active" by
  // platform UI toolkit messages, and those messages are not sent during unit
  // testing sessions.

  OpenURLsPopupObserver observer;
  BrowserList::AddObserver(&observer);

  Browser* popup = Browser::CreateForPopup(browser()->profile());
  ASSERT_EQ(popup->type(), Browser::TYPE_POPUP);
  ASSERT_EQ(popup, observer.added_browser_);

  CommandLine dummy(CommandLine::ARGUMENTS_ONLY);
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  // This should create a new window, but re-use the profile from |popup|. If
  // it used a NULL or invalid profile, it would crash.
  launch.OpenURLsInBrowser(popup, false, urls);
  ASSERT_NE(popup, observer.added_browser_);
  BrowserList::RemoveObserver(&observer);
}

// Test that we prevent openning potentially dangerous schemes from the
// command line.
// jschuh: DISABLED because the process doesn't have sufficient time
// to start on most BuildBot runs and I don't want to add longer delays to
// the test. I'll circle back and make this work properly when i get a chance.
IN_PROC_BROWSER_TEST_F(BrowserInitTest, DISABLED_BlockBadURLs) {
  const std::wstring testurlstr(L"http://localhost/");
  const GURL testurl(WideToUTF16Hack(testurlstr));
  CommandLine cmdline(CommandLine::ARGUMENTS_ONLY);
  cmdline.AppendLooseValue(testurlstr);
  cmdline.AppendLooseValue(std::wstring(L"javascript:alert('boo')"));
  cmdline.AppendLooseValue(testurlstr);
  cmdline.AppendLooseValue(std::wstring(L"view-source:http://localhost/"));

  // This will pick up the current browser instance.
  BrowserInit::LaunchWithProfile launch(std::wstring(), cmdline);
  launch.Launch(browser()->profile(), false);

  // Give the browser a chance to start first. FIXME(jschuh)
  PlatformThread::Sleep(50);

  // Skip about:blank in the first tab
  for (int  i = 1; i < browser()->tab_count(); i++) {
    const GURL &url = browser()->GetTabContentsAt(i)->GetURL();
    ASSERT_EQ(url, testurl);
  }
  ASSERT_EQ(browser()->tab_count(), 3);
}


}  // namespace
