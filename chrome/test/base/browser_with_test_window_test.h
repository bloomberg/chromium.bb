// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
#define CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_

#include "base/at_exit.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class GURL;

#if defined(USE_ASH)
namespace ash {
namespace test {
class AshTestHelper;
}
}
#endif

#if defined(USE_AURA)
namespace aura {
namespace test {
class AuraTestHelper;
}
}
#endif

#if defined(TOOLKIT_VIEWS)
namespace views {
class ViewsDelegate;
}
#endif

namespace content {
class NavigationController;
class WebContents;
}

// Base class for browser based unit tests. BrowserWithTestWindowTest creates a
// Browser with a TestingProfile and TestBrowserWindow. To add a tab use
// AddTab. For example, the following adds a tab and navigates to
// two URLs that target the TestWebContents:
//
//   // Add a new tab and navigate it. This will be at index 0.
//   AddTab(browser(), GURL("http://foo/1"));
//   NavigationController* controller =
//       &browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
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
  // Creates a BrowserWithTestWindowTest for which the initial window will be
  // a tabbed browser created on the native desktop, which is not a hosted app.
  BrowserWithTestWindowTest();

  // Creates a BrowserWithTestWindowTest for which the initial window will be
  // the specified type.
  BrowserWithTestWindowTest(Browser::Type browser_type,
                            chrome::HostDesktopType host_desktop_type,
                            bool hosted_app);

  virtual ~BrowserWithTestWindowTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  BrowserWindow* window() const { return window_.get(); }

  Browser* browser() const { return browser_.get(); }
  void set_browser(Browser* browser) {
    browser_.reset(browser);
  }
  Browser* release_browser() WARN_UNUSED_RESULT {
    return browser_.release();
  }

  TestingProfile* profile() const { return profile_; }

  TestingProfile* GetProfile() { return profile_; }

  BrowserWindow* release_browser_window() WARN_UNUSED_RESULT {
    return window_.release();
  }

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

  // Set the |title| of the current tab.
  void NavigateAndCommitActiveTabWithTitle(Browser* browser,
                                           const GURL& url,
                                           const base::string16& title);

  // Destroys the browser, window, and profile created by this class. This is
  // invoked from the destructor.
  void DestroyBrowserAndProfile();

  // Creates the profile used by this test. The caller owns the return value.
  virtual TestingProfile* CreateProfile();

  // Destroys the profile which was created through |CreateProfile|.
  virtual void DestroyProfile(TestingProfile* profile);

  // Creates the BrowserWindow used by this test. The caller owns the return
  // value. Can return NULL to use the default window created by Browser.
  virtual BrowserWindow* CreateBrowserWindow();

  // Creates the browser given |profile|, |browser_type|, |hosted_app|,
  // |host_desktop_type| and |browser_window|. The caller owns the return value.
  virtual Browser* CreateBrowser(Profile* profile,
                                 Browser::Type browser_type,
                                 bool hosted_app,
                                 chrome::HostDesktopType host_desktop_type,
                                 BrowserWindow* browser_window);

 private:
#if !defined(OS_CHROMEOS) && defined(TOOLKIT_VIEWS)
  // Creates the ViewsDelegate to use, may be overriden to create a different
  // ViewsDelegate.
  views::ViewsDelegate* CreateViewsDelegate();
#endif

  // We need to create a MessageLoop, otherwise a bunch of things fails.
  content::TestBrowserThreadBundle thread_bundle_;
  base::ShadowingAtExitManager at_exit_manager_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

  // The profile will automatically be destroyed by TearDown using the
  // |DestroyProfile()| function - which can be overwritten by derived testing
  // frameworks.
  TestingProfile* profile_;
  scoped_ptr<BrowserWindow> window_;  // Usually a TestBrowserWindow.
  scoped_ptr<Browser> browser_;

  // The existence of this object enables tests via
  // RenderViewHostTester.
  content::RenderViewHostTestEnabler rvh_test_enabler_;

#if defined(USE_ASH)
  scoped_ptr<ash::test::AshTestHelper> ash_test_helper_;
#endif
#if defined(USE_AURA)
  scoped_ptr<aura::test::AuraTestHelper> aura_test_helper_;
#endif

#if defined(TOOLKIT_VIEWS)
  scoped_ptr<views::ViewsDelegate> views_delegate_;
#endif

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  // The type of browser to create (tabbed or popup).
  Browser::Type browser_type_;

  // The desktop to create the initial window on.
  chrome::HostDesktopType host_desktop_type_;

  // Whether the browser is part of a hosted app.
  bool hosted_app_;

  DISALLOW_COPY_AND_ASSIGN(BrowserWithTestWindowTest);
};

#endif  // CHROME_TEST_BASE_BROWSER_WITH_TEST_WINDOW_TEST_H_
