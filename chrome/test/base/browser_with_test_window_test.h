// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
#define CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
#pragma once

#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class GURL;

#if defined(USE_AURA)
namespace aura {
class RootWindow;
namespace test {
class TestActivationClient;
class TestStackingClient;
}
}
#endif

namespace content {
class NavigationController;
class WebContents;
}

// Base class for browser based unit tests. BrowserWithTestWindowTest creates a
// Browser with a TestingProfile and TestBrowserWindow. To add a tab use
// AddTab. For example, the following adds a tab and navigates to
// two URLs that target the TestTabContents:
//
//   // Add a new tab and navigate it. This will be at index 0.
//   AddTab(browser(), GURL("http://foo/1"));
//   NavigationController* controller =
//       &browser()->GetTabContentsAt(0)->GetController();
//
//   // Navigate somewhere else.
//   GURL url2("http://foo/2");
//   NavigateAndCommit(controller, url2);
//
//   // This is equivalent to the above, and lets you test pending navigations.
//   browser()->OpenURL(OpenURLParams(
//       GURL("http://foo/2"), GURL(), CURRENT_TAB,
//       content::PAGE_TRANSITION_TYPED, false));
//   CommitPendingLoad(controller);
//
// Subclasses must invoke BrowserWithTestWindowTest::SetUp as it is responsible
// for creating the various objects of this class.
class BrowserWithTestWindowTest : public testing::Test {
 public:
  BrowserWithTestWindowTest();
  virtual ~BrowserWithTestWindowTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  TestBrowserWindow* window() const { return window_.get(); }
  void set_window(TestBrowserWindow* window) {
    window_.reset(window);
  }

  Browser* browser() const { return browser_.get(); }
  void set_browser(Browser* browser) {
    browser_.reset(browser);
  }

  TestingProfile* profile() const { return profile_.get(); }
  void set_profile(TestingProfile* profile) {
    profile_.reset(profile);
  }

  MessageLoop* message_loop() { return &ui_loop_; }

  // Adds a tab to |browser| with the given URL and commits the load.
  // This is a convenience function. The new tab will be added at index 0.
  void AddTab(Browser* browser, const GURL& url);

  // Commits the pending load on the given controller. It will keep the
  // URL of the pending load. If there is no pending load, this does nothing.
  void CommitPendingLoad(content::NavigationController* controller);

  // Creates a pending navigation on the given navigation controller to the
  // given URL with the default parameters and the commits the load with a page
  // ID one larger than any seen. This emulates what happens on a new
  // navigation.
  void NavigateAndCommit(content::NavigationController* controller,
                         const GURL& url);

  // Navigates the current tab. This is a wrapper around NavigateAndCommit.
  void NavigateAndCommitActiveTab(const GURL& url);

 protected:
  // Destroys the browser and window created by this class. This is invoked from
  // the destructor.
  void DestroyBrowser();

  // Creates the profile used by this test. The caller owners the return value.
  virtual TestingProfile* CreateProfile();

 private:
  // We need to create a MessageLoop, otherwise a bunch of things fails.
  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_user_blocking_thread_;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<TestBrowserWindow> window_;
  scoped_ptr<Browser> browser_;

  // The existence of this object enables tests via
  // RenderViewHostTester.
  content::RenderViewHostTestEnabler rvh_test_enabler_;

#if defined(USE_AURA)
  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<aura::test::TestActivationClient> test_activation_client_;
  scoped_ptr<aura::test::TestStackingClient> test_stacking_client_;
#endif

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BrowserWithTestWindowTest);
};

#endif  // CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
