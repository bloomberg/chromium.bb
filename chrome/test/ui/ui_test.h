// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_TEST_H_
#define CHROME_TEST_UI_UI_TEST_H_
#pragma once

// This file provides a common base for running UI unit tests, which operate
// the entire browser application in a separate process for holistic
// functional testing.
//
// Tests should #include this file, subclass UITest, and use the TEST_F macro
// to declare individual test cases.  This provides a running browser window
// during the test, accessible through the window_ member variable.  The window
// will close when the test ends, regardless of whether the test passed.
//
// Tests which need to launch the browser with a particular set of command-line
// arguments should set the value of launch_arguments_ in their constructors.

#include <string>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "build/build_config.h"
// TODO(evanm): we should be able to just forward-declare
// AutomationProxy here, but many files that #include this one don't
// themselves #include automation_proxy.h.
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/test_timeouts.h"
#include "testing/platform_test.h"

class AutomationProxy;
class BrowserProxy;
class DictionaryValue;
class FilePath;
class GURL;
class ScopedTempDir;
class TabProxy;

// Base class for UI Tests. This implements the core of the functions.
// This base class decouples all automation functionality from testing
// infrastructure, for use without gtest.
// If using gtest, you probably want to inherit from UITest (declared below)
// rather than UITestBase.
class UITestBase {
 protected:
  // String to display when a test fails because the crash service isn't
  // running.
  static const wchar_t kFailedNoCrashService[];

  // Constructor
  UITestBase();
  explicit UITestBase(MessageLoop::Type msg_loop_type);

  virtual ~UITestBase();

  // Starts the browser using the arguments in launch_arguments_, and
  // sets up member variables.
  virtual void SetUp();

  // Closes the browser window.
  virtual void TearDown();

 public:
  // ********* Utility functions *********

  // Launches the browser and IPC testing server.
  void LaunchBrowserAndServer();

  // Only for pyauto.
  void set_command_execution_timeout_ms(int timeout);

  // Overridable so that derived classes can provide their own AutomationProxy.
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout);

  // Closes the browser and IPC testing server.
  void CloseBrowserAndServer();

  // Launches the browser with the given command line.
  // TODO(phajdan.jr): Make LaunchBrowser private. Tests should use
  // LaunchAnotherBrowserBlockUntilClosed.
  void LaunchBrowser(const CommandLine& cmdline, bool clear_profile);

#if !defined(OS_MACOSX)
  // This function is deliberately not defined on the Mac because re-using an
  // existing browser process when launching from the command line isn't a
  // concept that we support on the Mac; AppleEvents are the Mac solution for
  // the same need. Any test based on this function doesn't apply to the Mac.

  // Launches an another browser process and waits for it to finish. Returns
  // true on success.
  bool LaunchAnotherBrowserBlockUntilClosed(const CommandLine& cmdline);
#endif

  // Exits out browser instance.
  void QuitBrowser();

  // Terminates the browser, simulates end of session.
  void TerminateBrowser();

  // Tells the browser to navigato to the givne URL in the active tab
  // of the first app window.
  // Does not wait for the navigation to complete to return.
  void NavigateToURLAsync(const GURL& url);

  // Tells the browser to navigate to the given URL in the active tab
  // of the first app window.
  // This method doesn't return until the navigation is complete.
  void NavigateToURL(const GURL& url);

  // Same as above, except in the given tab and window.
  void NavigateToURL(const GURL& url, int window_index, int tab_index);

  // Tells the browser to navigate to the given URL in the active tab
  // of the first app window.
  // This method doesn't return until the |number_of_navigations| navigations
  // complete.
  void NavigateToURLBlockUntilNavigationsComplete(const GURL& url,
                                                  int number_of_navigations);

  // Same as above, except in the given tab and window.
  void NavigateToURLBlockUntilNavigationsComplete(const GURL& url,
      int number_of_navigations, int tab_index, int window_index);

  // Returns the URL of the currently active tab. Only looks in the first
  // window, for backward compatibility. If there is no active tab, or some
  // other error, the returned URL will be empty.
  GURL GetActiveTabURL() { return GetActiveTabURL(0); }

  // Like above, but looks at the window at the given index.
  GURL GetActiveTabURL(int window_index);

  // Returns the title of the currently active tab. Only looks in the first
  // window, for backward compatibility.
  std::wstring GetActiveTabTitle() { return GetActiveTabTitle(0); }

  // Like above, but looks at the window at the given index.
  std::wstring GetActiveTabTitle(int window_index);

  // Returns the tabstrip index of the currently active tab in the window at
  // the given index, or -1 on error. Only looks in the first window, for
  // backward compatibility.
  int GetActiveTabIndex() { return GetActiveTabIndex(0); }

  // Like above, but looks at the window at the given index.
  int GetActiveTabIndex(int window_index);

  // Returns true when the browser process is running, independent if any
  // renderer process exists or not. It will returns false if an user closed the
  // window or if the browser process died by itself.
  bool IsBrowserRunning();

  // Returns true when time_out_ms milliseconds have elapsed.
  // Returns false if the browser process died while waiting.
  bool CrashAwareSleep(int time_out_ms);

  // Returns the number of tabs in the first window.  If no windows exist,
  // causes a test failure and returns 0.
  int GetTabCount();

  // Same as GetTabCount(), except with the window at the given index.
  int GetTabCount(int window_index);

  // Polls the tab for the cookie_name cookie and returns once one of the
  // following conditions hold true:
  // - The cookie is of expected_value.
  // - The browser process died.
  // - The time_out value has been exceeded.
  bool WaitUntilCookieValue(TabProxy* tab, const GURL& url,
                            const char* cookie_name,
                            int time_out_ms,
                            const char* expected_value);
  // Polls the tab for the cookie_name cookie and returns once one of the
  // following conditions hold true:
  // - The cookie is set to any value.
  // - The browser process died.
  // - The time_out value has been exceeded.
  std::string WaitUntilCookieNonEmpty(TabProxy* tab,
                                      const GURL& url,
                                      const char* cookie_name,
                                      int time_out_ms);

  // Polls the tab for a JavaScript condition and returns once one of the
  // following conditions hold true:
  // - The JavaScript condition evaluates to true (return true).
  // - The browser process died (return false).
  // - The time_out value has been exceeded (return false).
  //
  // The JavaScript expression is executed in the context of the frame that
  // matches the provided xpath.
  bool WaitUntilJavaScriptCondition(TabProxy* tab,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& jscript,
                                    int time_out_ms);

  // Polls up to kWaitForActionMaxMsec ms to attain a specific tab count. Will
  // assert that the tab count is valid at the end of the wait.
  void WaitUntilTabCount(int tab_count);

  // Checks whether the download shelf is visible in the current browser, giving
  // it a chance to appear (we don't know the exact timing) while finishing as
  // soon as possible.
  bool WaitForDownloadShelfVisible(BrowserProxy* browser);

  // Checks whether the download shelf is invisible in the current browser,
  // giving it a chance to appear (we don't know the exact timing) while
  // finishing as soon as possible.
  bool WaitForDownloadShelfInvisible(BrowserProxy* browser);

  // Wait for the browser process to shut down on its own (i.e. as a result of
  // some action that your test has taken).
  bool WaitForBrowserProcessToQuit();

 private:
  // Waits for download shelf visibility or invisibility.
  bool WaitForDownloadShelfVisibilityChange(BrowserProxy* browser,
                                            bool wait_for_open);

 public:

  // Waits until the Find window has become fully visible (if |wait_for_open| is
  // true) or fully hidden (if |wait_for_open| is false). This function can time
  // out (return false) if the window doesn't appear within a specific time.
  bool WaitForFindWindowVisibilityChange(BrowserProxy* browser,
                                         bool wait_for_open);

  // Waits until the Bookmark bar has stopped animating and become fully visible
  // (if |wait_for_open| is true) or fully hidden (if |wait_for_open| is false).
  // This function can time out (in which case it returns false).
  bool WaitForBookmarkBarVisibilityChange(BrowserProxy* browser,
                                          bool wait_for_open);

  // Sends the request to close the browser without blocking.
  // This is so we can interact with dialogs opened on browser close,
  // e.g. the beforeunload confirm dialog.
  void CloseBrowserAsync(BrowserProxy* browser) const;

  // Closes the specified browser.  Returns true if the browser was closed.
  // This call is blocking.  |application_closed| is set to true if this was
  // the last browser window (and therefore as a result of it closing the
  // browser process terminated).  Note that in that case this method returns
  // after the browser process has terminated.
  bool CloseBrowser(BrowserProxy* browser, bool* application_closed) const;

  // Gets the directory for the currently active profile in the browser.
  FilePath GetDownloadDirectory();

  // Get the handle of browser process connected to the automation. This
  // function only retruns a reference to the handle so the caller does not
  // own the handle returned.
  base::ProcessHandle process() { return process_; }

  // Wait for |generated_file| to be ready and then compare it with
  // |original_file| to see if they're identical or not if |compare_file| is
  // true. If |need_equal| is true, they need to be identical. Otherwise,
  // they should be different. This function will delete the generated file if
  // the parameter |delete_generated_file| is true.
  void WaitForGeneratedFileAndCheck(const FilePath& generated_file,
                                    const FilePath& original_file,
                                    bool compare_files,
                                    bool need_equal,
                                    bool delete_generated_file);

  // Get/Set a flag to run the renderer in process when running the
  // tests.
  static bool in_process_renderer() { return in_process_renderer_; }
  static void set_in_process_renderer(bool value) {
    in_process_renderer_ = value;
  }

  // Get/Set a flag to run the renderer outside the sandbox when running the
  // tests
  static bool no_sandbox() { return no_sandbox_; }
  static void set_no_sandbox(bool value) {
    no_sandbox_ = value;
  }

  // Get/Set a flag to run with DCHECKs enabled in release.
  static bool enable_dcheck() { return enable_dcheck_; }
  static void set_enable_dcheck(bool value) {
    enable_dcheck_ = value;
  }

  // Get/Set a flag to dump the process memory without crashing on DCHECKs.
  static bool silent_dump_on_dcheck() { return silent_dump_on_dcheck_; }
  static void set_silent_dump_on_dcheck(bool value) {
    silent_dump_on_dcheck_ = value;
  }

  // Get/Set a flag to disable breakpad handling.
  static bool disable_breakpad() { return disable_breakpad_; }
  static void set_disable_breakpad(bool value) {
    disable_breakpad_ = value;
  }

  // Get/Set a flag to run the plugin processes inside the sandbox when running
  // the tests
  static bool safe_plugins() { return safe_plugins_; }
  static void set_safe_plugins(bool value) {
    safe_plugins_ = value;
  }

  static bool show_error_dialogs() { return show_error_dialogs_; }
  static void set_show_error_dialogs(bool value) {
    show_error_dialogs_ = value;
  }

  static bool full_memory_dump() { return full_memory_dump_; }
  static void set_full_memory_dump(bool value) {
    full_memory_dump_ = value;
  }

  static bool dump_histograms_on_exit() { return dump_histograms_on_exit_; }
  static void set_dump_histograms_on_exit(bool value) {
    dump_histograms_on_exit_ = value;
  }

  static const std::string& js_flags() { return js_flags_; }
  static void set_js_flags(const std::string& value) {
    js_flags_ = value;
  }

  static const std::string& log_level() { return log_level_; }
  static void set_log_level(const std::string& value) {
    log_level_ = value;
  }

  // Profile theme type choices.
  typedef enum {
    DEFAULT_THEME = 0,
    COMPLEX_THEME = 1,
    NATIVE_THEME = 2,
    CUSTOM_FRAME = 3,
    CUSTOM_FRAME_NATIVE_THEME = 4,
  } ProfileType;

  // Returns the directory name where the "typical" user data is that we use
  // for testing.
  static FilePath ComputeTypicalUserDataSource(ProfileType profile_type);

  // Rewrite the preferences file to point to the proper image directory.
  static void RewritePreferencesFile(const FilePath& user_data_dir);

  // Called by some tests that wish to have a base profile to start from. This
  // "user data directory" (containing one or more profiles) will be recursively
  // copied into the user data directory for the test and the files will be
  // evicted from the OS cache. To start with a blank profile, supply an empty
  // string (the default).
  const FilePath& template_user_data() const { return template_user_data_; }
  void set_template_user_data(const FilePath& template_user_data) {
    template_user_data_ = template_user_data;
  }

  // Return the user data directory being used by the browser instance in
  // UITest::SetUp().
  FilePath user_data_dir() const;

  // Return the process id of the browser process (-1 on error).
  base::ProcessId browser_process_id() const { return process_id_; }

  // Compatibility timeout accessors.
  // TODO(phajdan.jr): update callers and remove these.
  static int command_execution_timeout_ms() {
    return TestTimeouts::command_execution_timeout_ms();
  }
  static int action_timeout_ms() {
    return TestTimeouts::action_timeout_ms();
  }
  static int action_max_timeout_ms() {
    return TestTimeouts::action_max_timeout_ms();
  }
  static int sleep_timeout_ms() {
    // TODO(phajdan.jr): Fix all callers and remove sleep_timeout_ms.
    return TestTimeouts::action_timeout_ms();
  }
  static int test_timeout_ms() {
    return TestTimeouts::huge_test_timeout_ms();
  }

  void set_ui_test_name(const std::string& name) {
    ui_test_name_ = name;
  }

  // Fetch the state which determines whether the profile will be cleared on
  // next startup.
  bool get_clear_profile() const {
    return clear_profile_;
  }
  // Sets clear_profile_. Should be called before launching browser to have
  // any effect.
  void set_clear_profile(bool clear_profile) {
    clear_profile_ = clear_profile;
  }

  // Sets homepage_. Should be called before launching browser to have
  // any effect.
  void set_homepage(const std::string& homepage) {
    homepage_ = homepage;
  }

  // Different ways to quit the browser.
  typedef enum {
    WINDOW_CLOSE,
    USER_QUIT,
    SESSION_ENDING,
  } ShutdownType;

  // Sets the shutdown type, which defaults to WINDOW_CLOSE.
  void set_shutdown_type(ShutdownType value) {
    shutdown_type_ = value;
  }

  // Count the number of active browser processes launched by this test.
  // The count includes browser sub-processes.
  int GetBrowserProcessCount();

  // Get the number of crash dumps we've logged since the test started.
  int GetCrashCount();

  // Returns a copy of local state preferences. The caller is responsible for
  // deleting the returned object. Returns NULL if there is an error.
  DictionaryValue* GetLocalState();

  // Returns a copy of the default profile preferences. The caller is
  // responsible for deleting the returned object. Returns NULL if there is an
  // error.
  DictionaryValue* GetDefaultProfilePreferences();

  // Waits for the test case to finish.
  // ASSERTS if there are test failures.
  void WaitForFinish(const std::string &name,
                     const std::string &id, const GURL &url,
                     const std::string& test_complete_cookie,
                     const std::string& expected_cookie_value,
                     const int wait_time);

  // Wrapper around EvictFileFromSystemCache to retry 10 times in case of
  // error.
  // Apparently needed for Windows buildbots (to workaround an error when
  // file is in use).
  // TODO(phajdan.jr): Move to test_file_util if we need it in more places.
  bool EvictFileFromSystemCacheWrapper(const FilePath& path);

  // Synchronously launches local http server normally used to run LayoutTests.
  void StartHttpServer(const FilePath& root_directory);

  // Launches local http server on the specified port.
  void StartHttpServerWithPort(const FilePath& root_directory, int port);

  void StopHttpServer();

  // Use Chromium binaries from the given directory.
  void SetBrowserDirectory(const FilePath& dir);

 private:
  // Check that no processes related to Chrome exist, displaying
  // the given message if any do.
  void AssertAppNotRunning(const std::wstring& error_message);

 protected:
  AutomationProxy* automation() {
    EXPECT_TRUE(server_.get());
    return server_.get();
  }

  virtual bool ShouldFilterInet() {
    return true;
  }

  // Wait a certain amount of time for all the app processes to exit,
  // forcibly killing them if they haven't exited by then.
  // It has the side-effect of killing every browser window opened in your
  // session, even those unrelated in the test.
  void CleanupAppProcesses();

  // Returns the proxy for the currently active tab, or NULL if there is no
  // tab or there was some kind of error. Only looks at the first window, for
  // backward compatibility. The returned pointer MUST be deleted by the
  // caller if non-NULL.
  scoped_refptr<TabProxy> GetActiveTab();

  // Like above, but looks at the window at the given index.
  scoped_refptr<TabProxy> GetActiveTab(int window_index);

  // ********* Member variables *********

  FilePath browser_directory_;          // Path to the browser executable.
  FilePath test_data_directory_;        // Path to the unit test data.
  CommandLine launch_arguments_;        // Command to launch the browser
  size_t expected_errors_;              // The number of errors expected during
                                        // the run (generally 0).
  int expected_crashes_;                // The number of crashes expected during
                                        // the run (generally 0).
  std::string homepage_;                // Homepage used for testing.
  bool wait_for_initial_loads_;         // Wait for initial loads to complete
                                        // in SetUp() before running test body.
  base::TimeTicks browser_launch_time_; // Time when the browser was run.
  base::TimeDelta browser_quit_time_;   // How long the shutdown took.
  bool dom_automation_enabled_;         // This can be set to true to have the
                                        // test run the dom automation case.
  FilePath template_user_data_;         // See set_template_user_data().
  base::ProcessHandle process_;         // Handle to the first Chrome process.
  base::ProcessId process_id_;          // PID of |process_| (for debugging).
  static bool in_process_renderer_;     // true if we're in single process mode
  bool show_window_;                    // Determines if the window is shown or
                                        // hidden. Defaults to hidden.
  bool clear_profile_;                  // If true the profile is cleared before
                                        // launching. Default is true.
  bool include_testing_id_;             // Should we supply the testing channel
                                        // id on the command line? Default is
                                        // true.
  bool enable_file_cookies_;            // Enable file cookies, default is true.
  ProfileType profile_type_;            // Are we using a profile with a
                                        // complex theme?
  FilePath websocket_pid_file_;         // PID file for websocket server.
  ShutdownType shutdown_type_;          // The method for shutting down
                                        // the browser. Used in ShutdownTest.

 private:
  bool LaunchBrowserHelper(const CommandLine& arguments,
                           bool wait,
                           base::ProcessHandle* process);

  // We want to have a current history database when we start the browser so
  // things like the NTP will have thumbnails.  This method updates the dates
  // in the history to be more recent.
  void UpdateHistoryDates();

  base::Time test_start_time_;          // Time the test was started
                                        // (so we can check for new crash dumps)
  static bool no_sandbox_;
  static bool safe_plugins_;
  static bool full_memory_dump_;        // If true, write full memory dump
                                        // during crash.
  static bool show_error_dialogs_;      // If true, a user is paying attention
                                        // to the test, so show error dialogs.
  static bool dump_histograms_on_exit_;  // Include histograms in log on exit.
  static bool enable_dcheck_;           // Enable dchecks in release mode.
  static bool silent_dump_on_dcheck_;   // Dump process memory on dcheck without
                                        // crashing.
  static bool disable_breakpad_;        // Disable breakpad on the browser.
  static int timeout_ms_;               // Timeout in milliseconds to wait
                                        // for an test to finish.
  static std::string js_flags_;         // Flags passed to the JS engine.
  static std::string log_level_;        // Logging level.

  scoped_ptr<AutomationProxy> server_;

  std::string ui_test_name_;

  // We use a temporary directory for profile to avoid issues with being
  // unable to delete some files because they're in use, etc.
  scoped_ptr<ScopedTempDir> temp_profile_dir_;
};

class UITest : public UITestBase, public PlatformTest {
 protected:
  UITest() {}
  explicit UITest(MessageLoop::Type msg_loop_type)
    : UITestBase(), PlatformTest(), message_loop_(msg_loop_type) {
  }
  virtual void SetUp();
  virtual void TearDown();

  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout);

 private:
  MessageLoop message_loop_;  // Enables PostTask to main thread.
};

// These exist only to support the gTest assertion macros, and
// shouldn't be used in normal program code.
#ifdef UNIT_TEST
std::ostream& operator<<(std::ostream& out, const std::wstring& wstr);

template<typename T>
std::ostream& operator<<(std::ostream& out, const ::scoped_ptr<T>& ptr) {
  return out << ptr.get();
}
#endif  // UNIT_TEST

#endif  // CHROME_TEST_UI_UI_TEST_H_
