// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/browser_test.h"
#include "content/test/browser_test_base.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#endif  // defined(OS_CHROMEOS)

class Browser;
class CommandLine;
class Profile;

namespace content {
class ContentRendererClient;
class ResourceContext;
}

namespace net {
class RuleBasedHostResolverProc;
}

// Base class for tests wanting to bring up a browser in the unit test process.
// Writing tests with InProcessBrowserTest is slightly different than that of
// other tests. This is necessitated by InProcessBrowserTest running a message
// loop. To use InProcessBrowserTest do the following:
// . Use the macro IN_PROC_BROWSER_TEST_F to define your test.
// . Your test method is invoked on the ui thread. If you need to block until
//   state changes you'll need to run the message loop from your test method.
//   For example, if you need to wait till a find bar has completely been shown
//   you'll need to invoke ui_test_utils::RunMessageLoop. When the message bar
//   is shown, invoke MessageLoop::current()->Quit() to return control back to
//   your test method.
// . If you subclass and override SetUp, be sure and invoke
//   InProcessBrowserTest::SetUp. (But see also SetUpOnMainThread,
//   SetUpInProcessBrowserTestFixture and other related hook methods for a
//   cleaner alternative).
//
// Following three hook methods are called in sequence before calling
// BrowserMain(), thus no browser has been created yet. They are mainly for
// setting up the environment for running the browser.
// . SetUpUserDataDirectory()
// . SetUpCommandLine()
// . SetUpInProcessBrowserTestFixture()
//
// SetUpOnMainThread() is called just after creating the default browser object
// and before executing the real test code. It's mainly for setting up things
// related to the browser object and associated window, like opening a new Tab
// with a testing page loaded.
//
// CleanUpOnMainThread() is called just after executing the real test code to
// do necessary cleanup before the browser is torn down.
//
// TearDownInProcessBrowserTestFixture() is called after BrowserMain() exits to
// cleanup things setup for running the browser.
//
// By default InProcessBrowserTest creates a single Browser (as returned from
// the CreateBrowser method). You can obviously create more as needed.

// InProcessBrowserTest disables the sandbox when running.
//
// See ui_test_utils for a handful of methods designed for use with this class.
class InProcessBrowserTest : public BrowserTestBase {
 public:
  InProcessBrowserTest();
  virtual ~InProcessBrowserTest();

  // Configures everything for an in process browser test, then invokes
  // BrowserMain. BrowserMain ends up invoking RunTestOnMainThreadLoop.
  virtual void SetUp() OVERRIDE;

  // Restores state configured in SetUp.
  virtual void TearDown() OVERRIDE;

 protected:
  // Returns the browser created by CreateBrowser.
  Browser* browser() const { return browser_; }

  // Returns the ResourceContext from browser_. Needed because tests in content
  // don't have access to Profile.
  const content::ResourceContext& GetResourceContext();

  // Convenience methods for adding tabs to a Browser.
  void AddTabAtIndexToBrowser(Browser* browser,
                              int index,
                              const GURL& url,
                              content::PageTransition transition);
  void AddTabAtIndex(int index, const GURL& url,
                     content::PageTransition transition);

  // Adds a selected tab at |index| to |url| with the specified |transition|.
  void AddTabAt(int index, const GURL& url, content::PageTransition transition);

  // Override this to add any custom setup code that needs to be done on the
  // main thread after the browser is created and just before calling
  // RunTestOnMainThread().
  virtual void SetUpOnMainThread() {}

  // Initializes the contents of the user data directory. Called by SetUp()
  // after creating the user data directory, but before any browser is launched.
  // If a test wishes to set up some initial non-empty state in the user data
  // directory before the browser starts up, it can do so here. Returns true if
  // successful.
  virtual bool SetUpUserDataDirectory() WARN_UNUSED_RESULT;

  // Override this to add command line flags specific to your test.
  virtual void SetUpCommandLine(CommandLine* command_line) {}

  // Override this to add any custom cleanup code that needs to be done on the
  // main thread before the browser is torn down.
  virtual void CleanUpOnMainThread() {}

  // BrowserTestBase:
  virtual void RunTestOnMainThreadLoop() OVERRIDE;

  // Returns the testing server. Guaranteed to be non-NULL.
  net::TestServer* test_server() { return test_server_.get(); }

  // Creates a browser with a single tab (about:blank), waits for the tab to
  // finish loading and shows the browser.
  //
  // This is invoked from Setup.
  virtual Browser* CreateBrowser(Profile* profile);

  // Similar to |CreateBrowser|, but creates an incognito browser.
  virtual Browser* CreateIncognitoBrowser();

  // Creates a browser for a popup window with a single tab (about:blank), waits
  // for the tab to finish loading, and shows the browser.
  Browser* CreateBrowserForPopup(Profile* profile);

  // Called from the various CreateBrowser methods to add a blank tab, wait for
  // the navigation to complete, and show the browser's window.
  void AddBlankTabAndShow(Browser* browser);

  // Returns the host resolver being used for the tests. Subclasses might want
  // to configure it inside tests.
  net::RuleBasedHostResolverProc* host_resolver() {
    return host_resolver_.get();
  }

  // Sets some test states (see below for comments).  Call this in your test
  // constructor.
  void set_show_window(bool show) { show_window_ = show; }
  void EnableDOMAutomation() { dom_automation_enabled_ = true; }
  void EnableTabCloseableStateWatcher() {
    tab_closeable_state_watcher_enabled_ = true;
  }

 private:
  // Creates a user data directory for the test if one is needed. Returns true
  // if successful.
  virtual bool CreateUserDataDirectory() WARN_UNUSED_RESULT;

  // Quits all open browsers and waits until there are no more browsers.
  void QuitBrowsers();

  // Prepare command line that will be used to launch the child browser process
  // with an in-process test.
  void PrepareTestCommandLine(CommandLine* command_line);

  // Browser created from CreateBrowser.
  Browser* browser_;

  // Testing server, started on demand.
  scoped_ptr<net::TestServer> test_server_;

  // ContentRendererClient when running in single-process mode.
  scoped_ptr<content::ContentRendererClient> single_process_renderer_client_;

  // Whether this test requires the browser windows to be shown (interactive
  // tests for example need the windows shown).
  bool show_window_;

  // Whether the JavaScript can access the DOMAutomationController (a JS object
  // that can send messages back to the browser).
  bool dom_automation_enabled_;

  // Whether this test requires the TabCloseableStateWatcher.
  bool tab_closeable_state_watcher_enabled_;

  // Host resolver to use during the test.
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_;

  // Temporary user data directory. Used only when a user data directory is not
  // specified in the command line.
  ScopedTempDir temp_user_data_dir_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
#endif  // defined(OS_CHROMEOS)
};

#endif  // CHROME_TEST_BASE_IN_PROCESS_BROWSER_TEST_H_
