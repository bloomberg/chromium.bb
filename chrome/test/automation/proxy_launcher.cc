// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/proxy_launcher.h"

#include "app/sql/connection.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/test_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "content/common/child_process_info.h"

namespace {

// Passed as value of kTestType.
const char kUITestType[] = "ui";

// Rewrite the preferences file to point to the proper image directory.
void RewritePreferencesFile(const FilePath& user_data_dir) {
  const FilePath pref_template_path(
      user_data_dir.AppendASCII("Default").AppendASCII("PreferencesTemplate"));
  const FilePath pref_path(
      user_data_dir.AppendASCII("Default").AppendASCII("Preferences"));

  // Read in preferences template.
  std::string pref_string;
  EXPECT_TRUE(file_util::ReadFileToString(pref_template_path, &pref_string));
  string16 format_string = ASCIIToUTF16(pref_string);

  // Make sure temp directory has the proper format for writing to prefs file.
#if defined(OS_POSIX)
  std::wstring user_data_dir_w(ASCIIToWide(user_data_dir.value()));
#elif defined(OS_WIN)
  std::wstring user_data_dir_w(user_data_dir.value());
  // In Windows, the FilePath will write '\' for the path separators; change
  // these to a separator that won't trigger escapes.
  std::replace(user_data_dir_w.begin(),
               user_data_dir_w.end(), '\\', '/');
#endif

  // Rewrite prefs file.
  std::vector<string16> subst;
  subst.push_back(WideToUTF16(user_data_dir_w));
  const std::string prefs_string =
      UTF16ToASCII(ReplaceStringPlaceholders(format_string, subst, NULL));
  EXPECT_TRUE(file_util::WriteFile(pref_path, prefs_string.c_str(),
                                   prefs_string.size()));
  file_util::EvictFileFromSystemCache(pref_path);
}

// We want to have a current history database when we start the browser so
// things like the NTP will have thumbnails.  This method updates the dates
// in the history to be more recent.
void UpdateHistoryDates(const FilePath& user_data_dir) {
  // Migrate the times in the segment_usage table to yesterday so we get
  // actual thumbnails on the NTP.
  sql::Connection db;
  FilePath history =
      user_data_dir.AppendASCII("Default").AppendASCII("History");
  // Not all test profiles have a history file.
  if (!file_util::PathExists(history))
    return;

  ASSERT_TRUE(db.Open(history));
  base::Time yesterday = base::Time::Now() - base::TimeDelta::FromDays(1);
  std::string yesterday_str = base::Int64ToString(yesterday.ToInternalValue());
  std::string query = StringPrintf(
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

const char ProxyLauncher::kDefaultInterfacePath[] =
    "/var/tmp/ChromeTestingInterface";

bool ProxyLauncher::in_process_renderer_ = false;
bool ProxyLauncher::no_sandbox_ = false;
bool ProxyLauncher::full_memory_dump_ = false;
bool ProxyLauncher::safe_plugins_ = false;
bool ProxyLauncher::show_error_dialogs_ = true;
bool ProxyLauncher::dump_histograms_on_exit_ = false;
bool ProxyLauncher::enable_dcheck_ = false;
bool ProxyLauncher::silent_dump_on_dcheck_ = false;
bool ProxyLauncher::disable_breakpad_ = false;
std::string ProxyLauncher::js_flags_ = "";
std::string ProxyLauncher::log_level_ = "";

ProxyLauncher::ProxyLauncher()
    : process_(base::kNullProcessHandle),
      process_id_(-1),
      shutdown_type_(WINDOW_CLOSE) {
}

ProxyLauncher::~ProxyLauncher() {}

void ProxyLauncher::WaitForBrowserLaunch(bool wait_for_initial_loads) {
  ASSERT_EQ(AUTOMATION_SUCCESS, automation_proxy_->WaitForAppLaunch())
      << "Error while awaiting automation ping from browser process";
  if (wait_for_initial_loads)
    ASSERT_TRUE(automation_proxy_->WaitForInitialLoads());
  else
    base::PlatformThread::Sleep(TestTimeouts::action_timeout_ms());

  EXPECT_TRUE(automation()->SetFilteredInet(ShouldFilterInet()));
}

void ProxyLauncher::LaunchBrowserAndServer(const LaunchState& state,
                                           bool wait_for_initial_loads) {
  // Set up IPC testing interface as a server.
  automation_proxy_.reset(CreateAutomationProxy(
                              TestTimeouts::command_execution_timeout_ms()));

  LaunchBrowser(state);
  WaitForBrowserLaunch(wait_for_initial_loads);
}

void ProxyLauncher::ConnectToRunningBrowser(bool wait_for_initial_loads) {
  // Set up IPC testing interface as a client.
  automation_proxy_.reset(CreateAutomationProxy(
                              TestTimeouts::command_execution_timeout_ms()));
  WaitForBrowserLaunch(wait_for_initial_loads);
}

void ProxyLauncher::CloseBrowserAndServer() {
  QuitBrowser();
  CleanupAppProcesses();

  // Suppress spammy failures that seem to be occurring when running
  // the UI tests in single-process mode.
  // TODO(jhughes): figure out why this is necessary at all, and fix it
  if (!in_process_renderer_)
    AssertAppNotRunning(
        StringPrintf(L"Unable to quit all browser processes. Original PID %d",
                     &process_id_));

  DisconnectFromRunningBrowser();
}

void ProxyLauncher::DisconnectFromRunningBrowser() {
  automation_proxy_.reset();  // Shut down IPC testing interface.
}

void ProxyLauncher::LaunchBrowser(const LaunchState& state) {
  if (state.clear_profile || !temp_profile_dir_.IsValid()) {
    if (temp_profile_dir_.IsValid())
      ASSERT_TRUE(temp_profile_dir_.Delete());
    ASSERT_TRUE(temp_profile_dir_.CreateUniqueTempDir());

    ASSERT_TRUE(test_launcher_utils::OverrideUserDataDir(user_data_dir()));
  }

  if (!state.template_user_data.empty()) {
    // Recursively copy the template directory to the user_data_dir.
    ASSERT_TRUE(file_util::CopyRecursiveDirNoCache(
        state.template_user_data,
        user_data_dir()));
    // If we're using the complex theme data, we need to write the
    // user_data_dir_ to our preferences file.
    if (state.profile_type == COMPLEX_THEME) {
      RewritePreferencesFile(user_data_dir());
    }

    // Update the history file to include recent dates.
    UpdateHistoryDates(user_data_dir());
  }

  ASSERT_TRUE(LaunchBrowserHelper(state, false, &process_));
  process_id_ = base::GetProcId(process_);
}

#if !defined(OS_MACOSX)
bool ProxyLauncher::LaunchAnotherBrowserBlockUntilClosed(
    const LaunchState& state) {
  return LaunchBrowserHelper(state, true, NULL);
}
#endif

void ProxyLauncher::QuitBrowser() {
  if (SESSION_ENDING == shutdown_type_) {
    TerminateBrowser();
    return;
  }

  // There's nothing to do here if the browser is not running.
  // WARNING: There is a race condition here where the browser may shut down
  // after this check but before some later automation call. Your test should
  // use WaitForBrowserProcessToQuit() if it intentionally
  // causes the browser to shut down.
  if (IsBrowserRunning()) {
    base::TimeTicks quit_start = base::TimeTicks::Now();
    EXPECT_TRUE(automation()->SetFilteredInet(false));

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
    automation()->Disconnect();

    // Wait for the browser process to quit. It should quit once all tabs have
    // been closed.
    if (!WaitForBrowserProcessToQuit(
        TestTimeouts::wait_for_terminate_timeout_ms())) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
    browser_quit_time_ = base::TimeTicks::Now() - quit_start;
  }

  // Don't forget to close the handle
  base::CloseProcessHandle(process_);
  process_ = base::kNullProcessHandle;
  process_id_ = -1;
}

void ProxyLauncher::TerminateBrowser() {
  if (IsBrowserRunning()) {
    base::TimeTicks quit_start = base::TimeTicks::Now();
    EXPECT_TRUE(automation()->SetFilteredInet(false));
#if defined(OS_WIN)
    scoped_refptr<BrowserProxy> browser(automation()->GetBrowserWindow(0));
    ASSERT_TRUE(browser.get());
    ASSERT_TRUE(browser->TerminateSession());
#endif  // defined(OS_WIN)

    // Now, drop the automation IPC channel so that the automation provider in
    // the browser notices and drops its reference to the browser process.
    automation()->Disconnect();

#if defined(OS_POSIX)
    EXPECT_EQ(kill(process_, SIGTERM), 0);
#endif  // OS_POSIX

    if (!WaitForBrowserProcessToQuit(
        TestTimeouts::wait_for_terminate_timeout_ms())) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
    browser_quit_time_ = base::TimeTicks::Now() - quit_start;
  }

  // Don't forget to close the handle
  base::CloseProcessHandle(process_);
  process_ = base::kNullProcessHandle;
  process_id_ = -1;
}

void ProxyLauncher::AssertAppNotRunning(const std::wstring& error_message) {
  std::wstring final_error_message(error_message);

  ChromeProcessList processes = GetRunningChromeProcesses(process_id_);
  if (!processes.empty()) {
    final_error_message += L" Leftover PIDs: [";
    for (ChromeProcessList::const_iterator it = processes.begin();
         it != processes.end(); ++it) {
      final_error_message += StringPrintf(L" %d", *it);
    }
    final_error_message += L" ]";
  }
  ASSERT_TRUE(processes.empty()) << final_error_message;
}

void ProxyLauncher::CleanupAppProcesses() {
  TerminateAllChromeProcesses(process_id_);
}

bool ProxyLauncher::WaitForBrowserProcessToQuit(int timeout) {
#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  timeout = 500000;
#endif
  return base::WaitForSingleProcess(process_, timeout);
}

bool ProxyLauncher::IsBrowserRunning() {
  return CrashAwareSleep(0);
}

bool ProxyLauncher::CrashAwareSleep(int timeout_ms) {
  return base::CrashAwareSleep(process_, timeout_ms);
}

void ProxyLauncher::PrepareTestCommandline(CommandLine* command_line,
                                           bool include_testing_id) {
  // Propagate commandline settings from test_launcher_utils.
  test_launcher_utils::PrepareBrowserCommandLineForTests(command_line);

  // Add any explicit command line flags passed to the process.
  CommandLine::StringType extra_chrome_flags =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kExtraChromeFlags);
  if (!extra_chrome_flags.empty()) {
    // Split by spaces and append to command line
    std::vector<CommandLine::StringType> flags;
    base::SplitString(extra_chrome_flags, ' ', &flags);
    for (size_t i = 0; i < flags.size(); ++i)
      command_line->AppendArgNative(flags[i]);
  }

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line->AppendSwitch(switches::kNoDefaultBrowserCheck);

  // This is a UI test.
  command_line->AppendSwitchASCII(switches::kTestType, kUITestType);

  // Tell the browser to use a temporary directory just for this test.
  command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir());

  if (include_testing_id)
    command_line->AppendSwitchASCII(switches::kTestingChannelID,
                                    PrefixedChannelID());

  if (!show_error_dialogs_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableErrorDialogs)) {
    command_line->AppendSwitch(switches::kNoErrorDialogs);
  }
  if (in_process_renderer_)
    command_line->AppendSwitch(switches::kSingleProcess);
  if (no_sandbox_)
    command_line->AppendSwitch(switches::kNoSandbox);
  if (full_memory_dump_)
    command_line->AppendSwitch(switches::kFullMemoryCrashReport);
  if (safe_plugins_)
    command_line->AppendSwitch(switches::kSafePlugins);
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

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  // Disable TabCloseableStateWatcher for tests.
  command_line->AppendSwitch(switches::kDisableTabCloseableStateWatcher);

  // Allow file:// access on ChromeOS.
  command_line->AppendSwitch(switches::kAllowFileAccess);
}

bool ProxyLauncher::LaunchBrowserHelper(const LaunchState& state, bool wait,
                                        base::ProcessHandle* process) {
  FilePath command = state.browser_directory.Append(
      chrome::kBrowserProcessExecutablePath);

  CommandLine command_line(command);

  // Add command line arguments that should be applied to all UI tests.
  PrepareTestCommandline(&command_line, state.include_testing_id);
  DebugFlags::ProcessDebugFlags(
      &command_line, ChildProcessInfo::UNKNOWN_PROCESS, false);
  command_line.AppendArguments(state.arguments, false);

  // TODO(phajdan.jr): Only run it for "main" browser launch.
  browser_launch_time_ = base::TimeTicks::Now();

#if defined(OS_WIN)
  bool started = base::LaunchApp(command_line, wait,
                                 !state.show_window, process);
#elif defined(OS_POSIX)
  // Sometimes one needs to run the browser under a special environment
  // (e.g. valgrind) without also running the test harness (e.g. python)
  // under the special environment.  Provide a way to wrap the browser
  // commandline with a special prefix to invoke the special environment.
  const char* browser_wrapper = getenv("BROWSER_WRAPPER");
  if (browser_wrapper) {
    command_line.PrependWrapper(browser_wrapper);
    VLOG(1) << "BROWSER_WRAPPER was set, prefixing command_line with "
            << browser_wrapper;
  }

  base::file_handle_mapping_vector fds;
  if (automation_proxy_.get())
    fds = automation_proxy_->fds_to_map();

  bool started = base::LaunchApp(command_line.argv(), fds, wait, process);
#endif

  return started;
}

AutomationProxy* ProxyLauncher::automation() const {
  EXPECT_TRUE(automation_proxy_.get());
  return automation_proxy_.get();
}

FilePath ProxyLauncher::user_data_dir() const {
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
    int execution_timeout) {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, true);
  return proxy;
}

void NamedProxyLauncher::InitializeConnection(const LaunchState& state,
                                              bool wait_for_initial_loads) {
  FilePath testing_channel_path;
#if defined(OS_WIN)
  testing_channel_path = FilePath(ASCIIToWide(channel_id_));
#else
  testing_channel_path = FilePath(channel_id_);
#endif

  if (launch_browser_) {
    // Because we are waiting on the existence of the testing file below,
    // make sure there isn't one already there before browser launch.
    EXPECT_TRUE(file_util::Delete(testing_channel_path, false));

    // Set up IPC testing interface as a client.
    LaunchBrowser(state);
  }

  // Wait for browser to be ready for connections.
  bool testing_channel_exists = false;
  for (int wait_time = 0;
       wait_time < TestTimeouts::command_execution_timeout_ms();
       wait_time += automation::kSleepTime) {
    testing_channel_exists = file_util::PathExists(testing_channel_path);
    if (testing_channel_exists)
      break;
    base::PlatformThread::Sleep(automation::kSleepTime);
  }
  EXPECT_TRUE(testing_channel_exists);

  ConnectToRunningBrowser(wait_for_initial_loads);
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
    int execution_timeout) {
  AutomationProxy* proxy = new AutomationProxy(execution_timeout,
                                               disconnect_on_failure_);
  proxy->InitializeChannel(channel_id_, false);
  return proxy;
}

void AnonymousProxyLauncher::InitializeConnection(const LaunchState& state,
                                                  bool wait_for_initial_loads) {
  LaunchBrowserAndServer(state, wait_for_initial_loads);
}

void AnonymousProxyLauncher::TerminateConnection() {
  CloseBrowserAndServer();
}

std::string AnonymousProxyLauncher::PrefixedChannelID() const {
  return channel_id_;
}
