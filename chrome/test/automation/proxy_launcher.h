// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_
#define CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/process.h"
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
  // Default ID for named testing interface.
  static const char kDefaultInterfaceId[];

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
    base::FilePath template_user_data;

    // Called just before starting the browser to allow any setup of the
    // profile for the run time environment.
    base::Closure setup_profile_callback;

    // Command line to launch the browser.
    CommandLine command;

    // Should we supply the testing channel id on the command line?
    bool include_testing_id;

    // If true, the window is shown. Otherwise it is hidden.
    bool show_window;
  };

  ProxyLauncher();

  virtual ~ProxyLauncher();

  // Launches the browser if needed and establishes a connection with it.
  // Returns true on success.
  virtual bool InitializeConnection(
      const LaunchState& state,
      bool wait_for_initial_loads) WARN_UNUSED_RESULT = 0;

  // Shuts down the browser if needed and destroys any
  // connections established by InitalizeConnection.
  virtual void TerminateConnection() = 0;

  // Launches the browser and IPC testing connection in server mode.
  // Returns true on success.
  bool LaunchBrowserAndServer(const LaunchState& state,
                              bool wait_for_initial_loads) WARN_UNUSED_RESULT;

  // Launches the IPC testing connection in client mode,
  // which then attempts to connect to a browser.
  // Returns true on success.
  bool ConnectToRunningBrowser(bool wait_for_initial_loads) WARN_UNUSED_RESULT;

  // Paired with LaunchBrowserAndServer().
  // Closes the browser and IPC testing server.
  void CloseBrowserAndServer();

  // Launches the browser with the given command line. Returns true on success.
  // TODO(phajdan.jr): Make LaunchBrowser private.
  bool LaunchBrowser(const LaunchState& state) WARN_UNUSED_RESULT;

  // Exits out of browser instance.
  void QuitBrowser();

  // Terminates the browser, simulates end of session.
  void TerminateBrowser();

  // Check that no processes related to Chrome exist, displaying
  // the given message if any do.
  void AssertAppNotRunning(const std::string& error_message);

  // Wait for the browser process to shut down on its own (i.e. as a result of
  // some action that your test has taken). If it has exited within |timeout|,
  // puts the exit code in |exit_code| and returns true.
  bool WaitForBrowserProcessToQuit(base::TimeDelta timeout, int* exit_code);

  AutomationProxy* automation() const;

  // Return the user data directory being used by the browser instance.
  base::FilePath user_data_dir() const;

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

  // Sets the shutdown type, which defaults to WINDOW_CLOSE.
  void set_shutdown_type(ShutdownType value) {
    shutdown_type_ = value;
  }

 protected:
  // Creates an automation proxy.
  virtual AutomationProxy* CreateAutomationProxy(
      base::TimeDelta execution_timeout) = 0;

  // Returns the automation proxy's channel with any prefixes prepended,
  // for passing as a command line parameter over to the browser.
  virtual std::string PrefixedChannelID() const = 0;

  // Paired with ConnectToRunningBrowser().
  // Disconnects the testing IPC from the browser.
  void DisconnectFromRunningBrowser();

 private:
  bool WaitForBrowserLaunch(bool wait_for_initial_loads) WARN_UNUSED_RESULT;

  // Prepare command line that will be used to launch the child browser process.
  void PrepareTestCommandline(CommandLine* command_line,
                              bool include_testing_id);

  bool LaunchBrowserHelper(const LaunchState& state,
                           bool main_launch,
                           bool wait,
                           base::ProcessHandle* process) WARN_UNUSED_RESULT;

  scoped_ptr<AutomationProxy> automation_proxy_;

  // We use a temporary directory for profile to avoid issues with being
  // unable to delete some files because they're in use, etc.
  base::ScopedTempDir temp_profile_dir_;

  // Handle to the first Chrome process.
  base::ProcessHandle process_;

  // PID of |process_| (for debugging).
  base::ProcessId process_id_;

  // Time when the browser was run.
  base::TimeTicks browser_launch_time_;

  // How long the shutdown took.
  base::TimeDelta browser_quit_time_;

  // The method for shutting down the browser. Used in ShutdownTest.
  ShutdownType shutdown_type_;

  // If true, runs the renderer outside the sandbox.
  bool no_sandbox_;

  // If true, write full memory dump during crash.
  bool full_memory_dump_;

  // If true, a user is paying attention to the test, so show error dialogs.
  bool show_error_dialogs_;

  // Enable dchecks in release mode.
  bool enable_dcheck_;

  // Dump process memory on dcheck without crashing.
  bool silent_dump_on_dcheck_;

  // Disable breakpad on the browser.
  bool disable_breakpad_;

  // Flags passed to the JS engine.
  std::string js_flags_;

  // Logging level.
  std::string log_level_;

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

  virtual AutomationProxy* CreateAutomationProxy(
      base::TimeDelta execution_timeout);
  virtual bool InitializeConnection(
      const LaunchState& state,
      bool wait_for_initial_loads) OVERRIDE WARN_UNUSED_RESULT;
  virtual void TerminateConnection();
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
  virtual AutomationProxy* CreateAutomationProxy(
      base::TimeDelta execution_timeout);
  virtual bool InitializeConnection(
      const LaunchState& state,
      bool wait_for_initial_loads) OVERRIDE WARN_UNUSED_RESULT;
  virtual void TerminateConnection();
  virtual std::string PrefixedChannelID() const;

 protected:
  std::string channel_id_;      // Channel id of automation proxy.
  bool disconnect_on_failure_;  // True if we disconnect on IPC channel failure.

 private:
  DISALLOW_COPY_AND_ASSIGN(AnonymousProxyLauncher);
};

#endif  // CHROME_TEST_AUTOMATION_PROXY_LAUNCHER_H_
