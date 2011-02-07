// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_
#define CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/page_transition_types.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#endif  // defined(OS_CHROMEOS)

class Browser;
class CommandLine;
class Profile;

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

// Browsers created while InProcessBrowserTest is running are shown hidden. Use
// the command line switch --show-windows to make them visible when debugging.
//
// InProcessBrowserTest disables the sandbox when running.
//
// See ui_test_utils for a handful of methods designed for use with this class.
class InProcessBrowserTest : public testing::Test {
 public:
  InProcessBrowserTest();
  virtual ~InProcessBrowserTest();

  // We do this so we can be used in a Task.
  void AddRef() {}
  void Release() {}
  static bool ImplementsThreadSafeReferenceCounting() { return false; }

  // Configures everything for an in process browser test, then invokes
  // BrowserMain. BrowserMain ends up invoking RunTestOnMainThreadLoop.
  virtual void SetUp();

  // Restores state configured in SetUp.
  virtual void TearDown();

 protected:
  // Returns the browser created by CreateBrowser.
  Browser* browser() const { return browser_; }

  // Convenience methods for adding tabs to a Browser.
  void AddTabAtIndexToBrowser(Browser* browser,
                              int index,
                              const GURL& url,
                              PageTransition::Type transition);
  void AddTabAtIndex(int index, const GURL& url,
                     PageTransition::Type transition);

  // Adds a selected tab at |index| to |url| with the specified |transition|.
  void AddTabAt(int index, const GURL& url, PageTransition::Type transition);

  // Override this to add any custom setup code that needs to be done on the
  // main thread after the browser is created and just before calling
  // RunTestOnMainThread().
  virtual void SetUpOnMainThread() {}

  // Override this rather than TestBody.
  virtual void RunTestOnMainThread() = 0;

  // Initializes the contents of the user data directory. Called by SetUp()
  // after creating the user data directory, but before any browser is launched.
  // If a test wishes to set up some initial non-empty state in the user data
  // directory before the browser starts up, it can do so here. Returns true if
  // successful.
  virtual bool SetUpUserDataDirectory() WARN_UNUSED_RESULT;

  // We need these special methods because InProcessBrowserTest::SetUp is the
  // bottom of the stack that winds up calling your test method, so it is not
  // always an option to do what you want by overriding it and calling the
  // superclass version.
  //
  // Override this for things you would normally override SetUp for. It will be
  // called before your individual test fixture method is run, but after most
  // of the overhead initialization has occured.
  virtual void SetUpInProcessBrowserTestFixture() {}

  // Override this for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture() {}

  // Override this to add command line flags specific to your test.
  virtual void SetUpCommandLine(CommandLine* command_line) {}

  // Override this to add any custom cleanup code that needs to be done on the
  // main thread before the browser is torn down.
  virtual void CleanUpOnMainThread() {}

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

  // This is invoked from main after browser_init/browser_main have completed.
  // This prepares for the test by creating a new browser, runs the test
  // (RunTestOnMainThread), quits the browsers and returns.
  void RunTestOnMainThreadLoop();

  // Quits all open browsers and waits until there are no more browsers.
  void QuitBrowsers();

  // Prepare command line that will be used to launch the child browser process
  // with an in-process test.
  void PrepareTestCommandLine(CommandLine* command_line);

  // Browser created from CreateBrowser.
  Browser* browser_;

  // Testing server, started on demand.
  scoped_ptr<net::TestServer> test_server_;

  // Whether this test requires the browser windows to be shown (interactive
  // tests for example need the windows shown).
  bool show_window_;

  // Whether the JavaScript can access the DOMAutomationController (a JS object
  // that can send messages back to the browser).
  bool dom_automation_enabled_;

  // Whether this test requires the TabCloseableStateWatcher.
  bool tab_closeable_state_watcher_enabled_;

  // We muck with the global command line for this process.  Keep the original
  // so we can reset it when we're done.  This matters when running the browser
  // tests in "single process" (all tests in one process) mode.
  scoped_ptr<CommandLine> original_command_line_;

  // Saved to restore the value of RenderProcessHost::run_renderer_in_process.
  bool original_single_process_;

  // Host resolver to use during the test.
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_;

  // Temporary user data directory. Used only when a user data directory is not
  // specified in the command line.
  ScopedTempDir temp_user_data_dir_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
#endif  // defined(OS_CHROMEOS)

  DISALLOW_COPY_AND_ASSIGN(InProcessBrowserTest);
};

// We only want to use IN_PROC_BROWSER_TEST in binaries which will properly
// isolate each test case. Otherwise hard-to-debug, possibly intermittent
// crashes caused by carrying state in singletons are very likely.
#if defined(ALLOW_IN_PROC_BROWSER_TEST)

#define IN_PROC_BROWSER_TEST_(test_case_name, test_name, parent_class,\
                              parent_id)\
class GTEST_TEST_CLASS_NAME_(test_case_name, test_name) : public parent_class {\
 public:\
  GTEST_TEST_CLASS_NAME_(test_case_name, test_name)() {}\
 protected:\
  virtual void RunTestOnMainThread();\
 private:\
  virtual void TestBody() {}\
  static ::testing::TestInfo* const test_info_;\
  GTEST_DISALLOW_COPY_AND_ASSIGN_(\
      GTEST_TEST_CLASS_NAME_(test_case_name, test_name));\
};\
\
::testing::TestInfo* const GTEST_TEST_CLASS_NAME_(test_case_name, test_name)\
  ::test_info_ =\
    ::testing::internal::MakeAndRegisterTestInfo(\
        #test_case_name, #test_name, "", "", \
        (parent_id), \
        parent_class::SetUpTestCase, \
        parent_class::TearDownTestCase, \
        new ::testing::internal::TestFactoryImpl<\
            GTEST_TEST_CLASS_NAME_(test_case_name, test_name)>);\
void GTEST_TEST_CLASS_NAME_(test_case_name, test_name)::RunTestOnMainThread()

#define IN_PROC_BROWSER_TEST_F(test_fixture, test_name)\
  IN_PROC_BROWSER_TEST_(test_fixture, test_name, test_fixture,\
                    ::testing::internal::GetTypeId<test_fixture>())

#endif  // defined(ALLOW_IN_PROC_BROWSER_TEST)

#endif  // CHROME_TEST_IN_PROCESS_BROWSER_TEST_H_
