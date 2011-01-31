// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_
#define CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/process.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"

class AutomationProxy;

// Base class for all ProxyLauncher implementations. Contains functionality
// fo launching, terminating, and connecting tests to browser processes. This
// class determines which AutomationProxy implementation is used by a test.
// Command line arguments passed to the browser are set in this class.
//
// Subclass from this class to use a different AutomationProxy
// implementation or to override browser launching behavior.
class ProxyLauncher {
 public:
  // Default path for named testing interface.
  static const char kDefaultInterfacePath[];

  // Profile theme type choices.
  enum ProfileType {
    DEFAULT_THEME = 0,
    COMPLEX_THEME = 1,
    NATIVE_THEME = 2,
    CUSTOM_FRAME = 3,
    CUSTOM_FRAME_NATIVE_THEME = 4,
  };

  // Different ways to quit the browser.
  enum ShutdownType {
    WINDOW_CLOSE,
    USER_QUIT,
    SESSION_ENDING,
  };

  // POD containing state variables that determine how to launch browser.
  struct LaunchState {
    // If true the profile is cleared before launching.
    bool clear_profile;

    // If set, the profiles in this path are copied
    // into the user data directory for the test.
    FilePath template_user_data;

    // Profile theme type.
    ProfileType profile_type;

    // Path to the browser executable.
    FilePath browser_directory;

    // Command line arguments passed to the browser.
    CommandLine arguments;

    // Should we supply the testing channel id on the command line?
    bool include_testing_id;

    // If true, the window is shown. Otherwise it is hidden.
    bool show_window;
  };

  ProxyLauncher();

  virtual ~ProxyLauncher();

  // Creates an automation proxy.
  virtual AutomationProxy* CreateAutomationProxy(
      int execution_timeout) = 0;

  // Launches the browser if needed and establishes a connection with it.
  virtual void InitializeConnection(const LaunchState& state,
                                    bool wait_for_initial_loads) = 0;

  // Returns the automation proxy's channel with any prefixes prepended,
  // for passing as a command line parameter over to the browser.
  virtual std::string PrefixedChannelID() const = 0;

  // Launches the browser and IPC testing connection in server mode.
  void LaunchBrowserAndServer(const LaunchState& state,
                              bool wait_for_initial_loads);

  // Launches the IPC testing connection in client mode,
  // which then attempts to connect to a browser.
  void ConnectToRunningBrowser(bool wait_for_initial_loads);

  // Only for pyauto.
  void set_command_execution_timeout_ms(int timeout);

  // Closes the browser and IPC testing server.
  void CloseBrowserAndServer(ShutdownType shutdown_type);

  // Launches the browser with the given command line.
  // TODO(phajdan.jr): Make LaunchBrowser private. Tests should use
  // LaunchAnotherBrowserBlockUntilClosed.
  void LaunchBrowser(const LaunchState& state);

#if !defined(OS_MACOSX)
  // This function is not defined on the Mac because the concept
  // doesn't apply to Mac; you can't have N browser processes.

  // Launches another browser process and waits for it to finish.
  // Returns true on success.
  bool LaunchAnotherBrowserBlockUntilClosed(const LaunchState& state);
#endif

  // Exits out of browser instance.
  void QuitBrowser(ShutdownType shutdown_type);

  // Terminates the browser, simulates end of session.
  void TerminateBrowser();

  // Check that no processes related to Chrome exist, displaying
  // the given message if any do.
  void AssertAppNotRunning(const std::wstring& error_message);

  // Returns true when the browser process is running, independent if any
  // renderer process exists or not. It will returns false if an user closed the
  // window or if the browser process died by itself.
  bool IsBrowserRunning();

  // Returns true when timeout_ms milliseconds have elapsed.
  // Returns false if the browser process died while waiting.
  bool CrashAwareSleep(int timeout_ms);

  // Wait for the browser process to shut down on its own (i.e. as a result of
  // some action that your test has taken).
  bool WaitForBrowserProcessToQuit(int timeout);

  AutomationProxy* automation() const;

  // Return the user data directory being used by the browser instance.
  FilePath user_data_dir() const;

  // Get the handle of browser process connected to the automation. This
  // function only returns a reference to the handle so the caller does not
  // own the handle returned.
  base::ProcessHandle process() const;

  // Return the process id of the browser process (-1 on error).
  base::ProcessId process_id() const;

  // Return the time when the browser was run.
  base::TimeTicks browser_launch_time() const;

  // Return how long the shutdown took.
  base::TimeDelta browser_quit_time() const;

  // Get/Set a flag to run the renderer in-process when running the tests.
  static bool in_process_renderer() { return in_process_renderer_; }
  static void set_in_process_renderer(bool value) {
    in_process_renderer_ = value;
  }

  // Get/Set a flag to run the renderer outside the sandbox when running tests.
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

 protected:
  virtual bool ShouldFilterInet() {
    return true;
  }

 private:
  void WaitForBrowserLaunch(bool wait_for_initial_loads);

  // Prepare command line that will be used to launch the child browser process.
  void PrepareTestCommandline(CommandLine* command_line,
                              bool include_testing_id);

  bool LaunchBrowserHelper(const LaunchState& state, bool wait,
                           base::ProcessHandle* process);

  // Wait a certain amount of time for all the app processes to exit,
  // forcibly killing them if they haven't exited by then.
  // It has the side-effect of killing every browser window opened in your
  // session, even those unrelated in the test.
  void CleanupAppProcesses();

  scoped_ptr<AutomationProxy> automation_proxy_;

  // We use a temporary directory for profile to avoid issues with being
  // unable to delete some files because they're in use, etc.
  ScopedTempDir temp_profile_dir_;

  // Handle to the first Chrome process.
  base::ProcessHandle process_;

  // PID of |process_| (for debugging).
  base::ProcessId process_id_;

  // Time when the browser was run.
  base::TimeTicks browser_launch_time_;

  // How long the shutdown took.
  base::TimeDelta browser_quit_time_;

  // True if we're in single process mode.
  static bool in_process_renderer_;

  // If true, runs the renderer outside the sandbox.
  static bool no_sandbox_;

  // If true, runs plugin processes inside the sandbox.
  static bool safe_plugins_;

  // If true, write full memory dump during crash.
  static bool full_memory_dump_;

  // If true, a user is paying attention to the test, so show error dialogs.
  static bool show_error_dialogs_;

  // Include histograms in log on exit.
  static bool dump_histograms_on_exit_;

  // Enable dchecks in release mode.
  static bool enable_dcheck_;

  // Dump process memory on dcheck without crashing.
  static bool silent_dump_on_dcheck_;

  // Disable breakpad on the browser.
  static bool disable_breakpad_;

  // Flags passed to the JS engine.
  static std::string js_flags_;

  // Logging level.
  static std::string log_level_;

  DISALLOW_COPY_AND_ASSIGN(ProxyLauncher);
};

// Uses an automation proxy that communicates over a named socket.
// This is used if you want to connect an AutomationProxy
// to a browser process that is already running.
// The channel id of the proxy is a constant specified by kInterfacePath.
class NamedProxyLauncher : public ProxyLauncher {
 public:
  // If launch_browser is true, launches Chrome with named interface enabled.
  // Otherwise, there should be an existing instance the proxy can connect to.
  NamedProxyLauncher(const std::string& channel_id,
                     bool launch_browser, bool disconnect_on_failure);

  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout);
  virtual void InitializeConnection(const LaunchState& state,
                                    bool wait_for_initial_loads);
  virtual std::string PrefixedChannelID() const;

 protected:
  std::string channel_id_;      // Channel id of automation proxy.
  bool launch_browser_;         // True if we should launch the browser too.
  bool disconnect_on_failure_;  // True if we disconnect on IPC channel failure.

 private:
  DISALLOW_COPY_AND_ASSIGN(NamedProxyLauncher);
};

// Uses an automation proxy that communicates over an anonymous socket.
class AnonymousProxyLauncher : public ProxyLauncher {
 public:
  explicit AnonymousProxyLauncher(bool disconnect_on_failure);
  virtual AutomationProxy* CreateAutomationProxy(int execution_timeout);
  virtual void InitializeConnection(const LaunchState& state,
                                    bool wait_for_initial_loads);
  virtual std::string PrefixedChannelID() const;

 protected:
  std::string channel_id_;      // Channel id of automation proxy.
  bool disconnect_on_failure_;  // True if we disconnect on IPC channel failure.

 private:
  DISALLOW_COPY_AND_ASSIGN(AnonymousProxyLauncher);
};

#endif  // CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_
