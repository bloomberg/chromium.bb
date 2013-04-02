// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/proxy_launcher.h"

#include <vector>

#include "base/environment.h"
#include "base/file_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/ui/ui_test.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_descriptors.h"
#include "sql/connection.h"

#if defined(OS_POSIX)
#include <signal.h>
#endif

namespace {

// Passed as value of kTestType.
const char kUITestType[] = "ui";

// We want to have a current history database when we start the browser so
// things like the NTP will have thumbnails.  This method updates the dates
// in the history to be more recent.
void UpdateHistoryDates(const base::FilePath& user_data_dir) {
  // Migrate the times in the segment_usage table to yesterday so we get
  // actual thumbnails on the NTP.
  sql::Connection db;
  base::FilePath history =
      user_data_dir.AppendASCII("Default").AppendASCII("History");
  // Not all test profiles have a history file.
  if (!file_util::PathExists(history))
    return;

  ASSERT_TRUE(db.Open(history));
  base::Time yesterday = base::Time::Now() - base::TimeDelta::FromDays(1);
  std::string yesterday_str = base::Int64ToString(yesterday.ToInternalValue());
  std::string query = base::StringPrintf(
      "UPDATE segment_usage "
      "SET time_slot = %s "
      "WHERE id IN (SELECT id FROM segment_usage WHERE time_slot > 0);",
      yesterday_str.c_str());
  ASSERT_TRUE(db.Execute(query.c_str()));
  db.Close();
  file_util::EvictFileFromSystemCache(history);
}

}  // namespace

// ProxyLauncher functions

#if defined(OS_WIN)
const char ProxyLauncher::kDefaultInterfaceId[] = "ChromeTestingInterface";
#elif defined(OS_POSIX)
const char ProxyLauncher::kDefaultInterfaceId[] =
    "/var/tmp/ChromeTestingInterface";
#endif

ProxyLauncher::ProxyLauncher()
    : process_(base::kNullProcessHandle),
      process_id_(-1),
      shutdown_type_(WINDOW_CLOSE),
      no_sandbox_(CommandLine::ForCurrentProcess()->HasSwitch(
                      switches::kNoSandbox)),
      full_memory_dump_(CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kFullMemoryCrashReport)),
      show_error_dialogs_(CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kEnableErrorDialogs)),
      dump_histograms_on_exit_(CommandLine::ForCurrentProcess()->HasSwitch(
                                   switches::kDumpHistogramsOnExit)),
      enable_dcheck_(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kEnableDCHECK)),
      silent_dump_on_dcheck_(CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kSilentDumpOnDCHECK)),
      disable_breakpad_(CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kDisableBreakpad)),
      js_flags_(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                    switches::kJavaScriptFlags)),
      log_level_(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                     switches::kLoggingLevel)) {
}

ProxyLauncher::~ProxyLauncher() {}

bool ProxyLauncher::WaitForBrowserLaunch(bool wait_for_initial_loads) {
  AutomationLaunchResult app_launched = automation_proxy_->WaitForAppLaunch();
  EXPECT_EQ(AUTOMATION_SUCCESS, app_launched)
      << "Error while awaiting automation ping from browser process";
  if (app_launched != AUTOMATION_SUCCESS)
    return false;

  if (wait_for_initial_loads) {
    if (!automation_proxy_->WaitForInitialLoads()) {
      LOG(ERROR) << "WaitForInitialLoads failed.";
      return false;
    }
  } else {
#if defined(OS_WIN)
    // TODO(phajdan.jr): Get rid of this Sleep when logging_chrome_uitest
    // stops "relying" on it.
    base::PlatformThread::Sleep(TestTimeouts::action_timeout());
#endif
  }

  return true;
}

bool ProxyLauncher::LaunchBrowserAndServer(const LaunchState& state,
                                           bool wait_for_initial_loads) {
  // Set up IPC testing interface as a server.
  automation_proxy_.reset(CreateAutomationProxy(
      TestTimeouts::action_max_timeout()));

  if (!LaunchBrowser(state))
    return false;

  if (!WaitForBrowserLaunch(wait_for_initial_loads))
    return false;

  return true;
}

bool ProxyLauncher::ConnectToRunningBrowser(bool wait_for_initial_loads) {
  // Set up IPC testing interface as a client.
  automation_proxy_.reset(CreateAutomationProxy(
                              TestTimeouts::action_max_timeout()));

  return WaitForBrowserLaunch(wait_for_initial_loads);
}

void ProxyLauncher::CloseBrowserAndServer() {
  QuitBrowser();

  // Suppress spammy failures that seem to be occurring when running
  // the UI tests in single-process mode.
  // TODO(jhughes): figure out why this is necessary at all, and fix it
  AssertAppNotRunning(
      base::StringPrintf(
          "Unable to quit all browser processes. Original PID %d",
          process_id_));

  DisconnectFromRunningBrowser();
}

void ProxyLauncher::DisconnectFromRunningBrowser() {
  automation_proxy_.reset();  // Shut down IPC testing interface.
}

bool ProxyLauncher::LaunchBrowser(const LaunchState& state) {
  if (state.clear_profile || !temp_profile_dir_.IsValid()) {
    if (temp_profile_dir_.IsValid() && !temp_profile_dir_.Delete()) {
      LOG(ERROR) << "Failed to delete temporary directory.";
      return false;
    }

    if (!temp_profile_dir_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Failed to create temporary directory.";
      return false;
    }

    if (!test_launcher_utils::OverrideUserDataDir(user_data_dir())) {
      LOG(ERROR) << "Failed to override user data directory.";
      return false;
    }
  }

  if (!state.template_user_data.empty()) {
    // Recursively copy the template directory to the user_data_dir.
    if (!file_util::CopyDirectory(state.template_user_data, user_data_dir(),
                                  true)) {
      LOG(ERROR) << "Failed to copy user data directory template.";
      return false;
    }

    // Update the history file to include recent dates.
    UpdateHistoryDates(user_data_dir());
  }

  // Optionally do any final setup of the test environment.
  if (!state.setup_profile_callback.is_null())
    state.setup_profile_callback.Run();

  if (!LaunchBrowserHelper(state, true, false, &process_)) {
    LOG(ERROR) << "LaunchBrowserHelper failed.";
    return false;
  }
  process_id_ = base::GetProcId(process_);

  return true;
}

void ProxyLauncher::QuitBrowser() {
  // If we have already finished waiting for the browser to exit
  // (or it hasn't launched at all), there's nothing to do here.
  if (process_ == base::kNullProcessHandle || !automation_proxy_.get())
    return;

  if (SESSION_ENDING == shutdown_type_) {
    TerminateBrowser();
    return;
  }

  base::TimeTicks quit_start = base::TimeTicks::Now();

  if (WINDOW_CLOSE == shutdown_type_) {
    int window_count = 0;
    EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));

    // Synchronously close all but the last browser window. Closing them
    // one-by-one may help with stability.
    while (window_count > 1) {
      scoped_refptr<BrowserProxy> browser_proxy =
          automation()->GetBrowserWindow(0);
      EXPECT_TRUE(browser_proxy.get());
      if (browser_proxy.get()) {
        EXPECT_TRUE(browser_proxy->RunCommand(IDC_CLOSE_WINDOW));
        EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
      } else {
        break;
      }
    }

    // Close the last window asynchronously, because the browser may
    // shutdown faster than it will be able to send a synchronous response
    // to our message.
    scoped_refptr<BrowserProxy> browser_proxy =
        automation()->GetBrowserWindow(0);
    EXPECT_TRUE(browser_proxy.get());
    if (browser_proxy.get()) {
      EXPECT_TRUE(browser_proxy->is_valid());
      EXPECT_TRUE(browser_proxy->ApplyAccelerator(IDC_CLOSE_WINDOW));
      browser_proxy = NULL;
    }
  } else if (USER_QUIT == shutdown_type_) {
    scoped_refptr<BrowserProxy> browser_proxy =
        automation()->GetBrowserWindow(0);
    EXPECT_TRUE(browser_proxy.get());
    if (browser_proxy.get()) {
      EXPECT_TRUE(browser_proxy->RunCommandAsync(IDC_EXIT));
    }
  } else {
    NOTREACHED() << "Invalid shutdown type " << shutdown_type_;
  }

  // Now, drop the automation IPC channel so that the automation provider in
  // the browser notices and drops its reference to the browser process.
  if (automation_proxy_.get())
    automation_proxy_->Disconnect();

  // Wait for the browser process to quit. It should quit once all tabs have
  // been closed.
  int exit_code = -1;
  EXPECT_TRUE(WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutdown.

  browser_quit_time_ = base::TimeTicks::Now() - quit_start;
}

void ProxyLauncher::TerminateBrowser() {
  // If we have already finished waiting for the browser to exit
  // (or it hasn't launched at all), there's nothing to do here.
  if (process_ == base::kNullProcessHandle || !automation_proxy_.get())
    return;

  base::TimeTicks quit_start = base::TimeTicks::Now();

#if defined(OS_WIN) && !defined(USE_AURA)
  scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
  ASSERT_TRUE(browser.get());
  ASSERT_TRUE(browser->TerminateSession());
#endif  // defined(OS_WIN)

  // Now, drop the automation IPC channel so that the automation provider in
  // the browser notices and drops its reference to the browser process.
  if (automation_proxy_.get())
    automation_proxy_->Disconnect();

#if defined(OS_POSIX)
  EXPECT_EQ(kill(process_, SIGTERM), 0);
#endif  // OS_POSIX

  int exit_code = -1;
  EXPECT_TRUE(WaitForBrowserProcessToQuit(
                  TestTimeouts::action_max_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);  // Expect a clean shutdown.

  browser_quit_time_ = base::TimeTicks::Now() - quit_start;
}

void ProxyLauncher::AssertAppNotRunning(const std::string& error_message) {
  std::string final_error_message(error_message);

  ChromeProcessList processes = GetRunningChromeProcesses(process_id_);
  if (!processes.empty()) {
    final_error_message += " Leftover PIDs: [";
    for (ChromeProcessList::const_iterator it = processes.begin();
         it != processes.end(); ++it) {
      final_error_message += base::StringPrintf(" %d", *it);
    }
    final_error_message += " ]";
  }
  ASSERT_TRUE(processes.empty()) << final_error_message;
}

bool ProxyLauncher::WaitForBrowserProcessToQuit(
    base::TimeDelta timeout,
    int* exit_code) {
#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  timeout = base::TimeDelta::FromSeconds(500);
#endif
  bool success = false;

  // Only wait for exit if the "browser, please terminate" message had a
  // chance of making it through.
  if (!automation_proxy_->channel_disconnected_on_failure())
    success = base::WaitForExitCodeWithTimeout(process_, exit_code, timeout);

  if (!success)
    TerminateAllChromeProcesses(process_id_);

  base::CloseProcessHandle(process_);
  process_ = base::kNullProcessHandle;
  process_id_ = -1;

  return success;
}

void ProxyLauncher::PrepareTestCommandline(CommandLine* command_line,
                                           bool include_testing_id) {
  // Add any explicit command line flags passed to the process.
  CommandLine::StringType extra_chrome_flags =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kExtraChromeFlags);
  if (!extra_chrome_flags.empty()) {
    // Split by spaces and append to command line.
    std::vector<CommandLine::StringType> flags;
    base::SplitStringAlongWhitespace(extra_chrome_flags, &flags);
    for (size_t i = 0; i < flags.size(); ++i)
      command_line->AppendArgNative(flags[i]);
  }

  // Also look for extra flags in environment.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string extra_from_env;
  if (env->GetVar("EXTRA_CHROME_FLAGS", &extra_from_env)) {
    std::vector<std::string> flags;
    base::SplitStringAlongWhitespace(extra_from_env, &flags);
    for (size_t i = 0; i < flags.size(); ++i)
      command_line->AppendArg(flags[i]);
  }

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line->AppendSwitch(switches::kNoDefaultBrowserCheck);

  // This is a UI test.
  command_line->AppendSwitchASCII(switches::kTestType, kUITestType);

  // Tell the browser to use a temporary directory just for this test
  // if it is not already set.
  if (command_line->GetSwitchValuePath(switches::kUserDataDir).empty()) {
    command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir());
  }

  if (include_testing_id)
    command_line->AppendSwitchASCII(switches::kTestingChannelID,
                                    PrefixedChannelID());

  if (!show_error_dialogs_)
    command_line->AppendSwitch(switches::kNoErrorDialogs);
  if (no_sandbox_)
    command_line->AppendSwitch(switches::kNoSandbox);
  if (full_memory_dump_)
    command_line->AppendSwitch(switches::kFullMemoryCrashReport);
  if (enable_dcheck_)
    command_line->AppendSwitch(switches::kEnableDCHECK);
  if (silent_dump_on_dcheck_)
    command_line->AppendSwitch(switches::kSilentDumpOnDCHECK);
  if (disable_breakpad_)
    command_line->AppendSwitch(switches::kDisableBreakpad);

  if (!js_flags_.empty())
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, js_flags_);
  if (!log_level_.empty())
    command_line->AppendSwitchASCII(switches::kLoggingLevel, log_level_);

  command_line->AppendSwitch(switches::kMetricsRecordingOnly);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableErrorDialogs))
    command_line->AppendSwitch(switches::kEnableLogging);

  if (dump_histograms_on_exit_)
    command_line->AppendSwitch(switches::kDumpHistogramsOnExit);

#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  command_line->AppendSwitch(switches::kDebugOnStart);
#endif

  // Force the app to always exit when the last browser window is closed.
  command_line->AppendSwitch(switches::kDisableZeroBrowsersOpenForTests);

  // Allow file:// access on ChromeOS.
  command_line->AppendSwitch(switches::kAllowFileAccess);

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);
}

bool ProxyLauncher::LaunchBrowserHelper(const LaunchState& state,
                                        bool main_launch,
                                        bool wait,
                                        base::ProcessHandle* process) {
  CommandLine command_line(state.command);

  // Add command line arguments that should be applied to all UI tests.
  PrepareTestCommandline(&command_line, state.include_testing_id);

  // Sometimes one needs to run the browser under a special environment
  // (e.g. valgrind) without also running the test harness (e.g. python)
  // under the special environment.  Provide a way to wrap the browser
  // commandline with a special prefix to invoke the special environment.
  const char* browser_wrapper = getenv("BROWSER_WRAPPER");
  if (browser_wrapper) {
#if defined(OS_WIN)
    command_line.PrependWrapper(ASCIIToWide(browser_wrapper));
#elif defined(OS_POSIX)
    command_line.PrependWrapper(browser_wrapper);
#endif
    VLOG(1) << "BROWSER_WRAPPER was set, prefixing command_line with "
            << browser_wrapper;
  }

  if (main_launch)
    browser_launch_time_ = base::TimeTicks::Now();

  base::LaunchOptions options;
  options.wait = wait;

#if defined(OS_WIN)
  options.start_hidden = !state.show_window;
#elif defined(OS_POSIX)
  int ipcfd = -1;
  file_util::ScopedFD ipcfd_closer(&ipcfd);
  base::FileHandleMappingVector fds;
  if (main_launch && automation_proxy_.get()) {
    ipcfd = automation_proxy_->channel()->TakeClientFileDescriptor();
    fds.push_back(std::make_pair(ipcfd, kPrimaryIPCChannel + 3));
    options.fds_to_remap = &fds;
  }
#endif

  return base::LaunchProcess(command_line, options, process);
}

AutomationProxy* ProxyLauncher::automation() const {
  EXPECT_TRUE(automation_proxy_.get());
  return automation_proxy_.get();
}

base::FilePath ProxyLauncher::user_data_dir() const {
  EXPECT_TRUE(temp_profile_dir_.IsValid());
  return temp_profile_dir_.path();
}

base::ProcessHandle ProxyLauncher::process() const {
  return process_;
}

base::ProcessId ProxyLauncher::process_id() const {
  return process_id_;
}

base::TimeTicks ProxyLauncher::browser_launch_time() const {
  return browser_launch_time_;
}

base::TimeDelta ProxyLauncher::browser_quit_time() const {
  return browser_quit_time_;
}

// NamedProxyLauncher functions

NamedProxyLauncher::NamedProxyLauncher(const std::string& channel_id,
                                       bool launch_browser,
                                       bool disconnect_on_failure)
    : channel_id_(channel_id),
      launch_browser_(launch_browser),
      disconnect_on_failure_(disconnect_on_failure) {
}

AutomationProxy* NamedProxyLauncher::CreateAutomationProxy(
    base::TimeDelta execution_timeout) {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, true);
  return proxy;
}

bool NamedProxyLauncher::InitializeConnection(const LaunchState& state,
                                              bool wait_for_initial_loads) {
  if (launch_browser_) {
#if defined(OS_POSIX)
    // Because we are waiting on the existence of the testing file below,
    // make sure there isn't one already there before browser launch.
    if (!file_util::Delete(base::FilePath(channel_id_), false)) {
      LOG(ERROR) << "Failed to delete " << channel_id_;
      return false;
    }
#endif

    if (!LaunchBrowser(state)) {
      LOG(ERROR) << "Failed to LaunchBrowser";
      return false;
    }
  }

  // Wait for browser to be ready for connections.
  bool channel_initialized = false;
  base::TimeDelta sleep_time = base::TimeDelta::FromMilliseconds(
      automation::kSleepTime);
  for (base::TimeDelta wait_time = base::TimeDelta();
       wait_time < TestTimeouts::action_max_timeout();
       wait_time += sleep_time) {
    channel_initialized = IPC::Channel::IsNamedServerInitialized(channel_id_);
    if (channel_initialized)
      break;
    base::PlatformThread::Sleep(sleep_time);
  }
  if (!channel_initialized) {
    LOG(ERROR) << "Failed to wait for testing channel presence.";
    return false;
  }

  if (!ConnectToRunningBrowser(wait_for_initial_loads)) {
    LOG(ERROR) << "Failed to ConnectToRunningBrowser";
    return false;
  }
  return true;
}

void NamedProxyLauncher::TerminateConnection() {
  if (launch_browser_)
    CloseBrowserAndServer();
  else
    DisconnectFromRunningBrowser();
}

std::string NamedProxyLauncher::PrefixedChannelID() const {
  std::string channel_id;
  channel_id.append(automation::kNamedInterfacePrefix).append(channel_id_);
  return channel_id;
}

// AnonymousProxyLauncher functions

AnonymousProxyLauncher::AnonymousProxyLauncher(bool disconnect_on_failure)
    : disconnect_on_failure_(disconnect_on_failure) {
  channel_id_ = AutomationProxy::GenerateChannelID();
}

AutomationProxy* AnonymousProxyLauncher::CreateAutomationProxy(
    base::TimeDelta execution_timeout) {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, false);
  return proxy;
}

bool AnonymousProxyLauncher::InitializeConnection(const LaunchState& state,
                                                  bool wait_for_initial_loads) {
  return LaunchBrowserAndServer(state, wait_for_initial_loads);
}

void AnonymousProxyLauncher::TerminateConnection() {
  CloseBrowserAndServer();
}

std::string AnonymousProxyLauncher::PrefixedChannelID() const {
  return channel_id_;
}
