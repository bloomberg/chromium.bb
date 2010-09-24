// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#if defined(OS_POSIX)
#include <signal.h>
#include <sys/types.h>
#endif

#include <set>
#include <vector>

#include "app/sql/connection.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/debug_flags.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/javascript_execution_controller.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/test_launcher_utils.h"
#include "chrome/test/test_switches.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_WIN)
#include "base/win_util.h"
#endif


using base::Time;
using base::TimeDelta;
using base::TimeTicks;

// Passed as value of kTestType.
static const char kUITestType[] = "ui";

const wchar_t UITestBase::kFailedNoCrashService[] =
#if defined(OS_WIN)
    L"NOTE: This test is expected to fail if crash_service.exe is not "
    L"running. Start it manually before running this test (see the build "
    L"output directory).";
#elif defined(OS_LINUX)
    L"NOTE: This test is expected to fail if breakpad is not built in "
    L"or if chromium is not running headless (try CHROME_HEADLESS=1).";
#else
    L"NOTE: Crash service not ported to this platform!";
#endif
bool UITestBase::in_process_renderer_ = false;
bool UITestBase::no_sandbox_ = false;
bool UITestBase::full_memory_dump_ = false;
bool UITestBase::safe_plugins_ = false;
bool UITestBase::show_error_dialogs_ = true;
bool UITestBase::dump_histograms_on_exit_ = false;
bool UITestBase::enable_dcheck_ = false;
bool UITestBase::silent_dump_on_dcheck_ = false;
bool UITestBase::disable_breakpad_ = false;
std::string UITestBase::js_flags_ = "";
std::string UITestBase::log_level_ = "";

// Uncomment this line to have the spawned process wait for the debugger to
// attach.  This only works on Windows.  On posix systems, you can set the
// BROWSER_WRAPPER env variable to wrap the browser process.
// #define WAIT_FOR_DEBUGGER_ON_OPEN 1

UITestBase::UITestBase()
    : launch_arguments_(CommandLine::ARGUMENTS_ONLY),
      expected_errors_(0),
      expected_crashes_(0),
      homepage_(chrome::kAboutBlankURL),
      wait_for_initial_loads_(true),
      dom_automation_enabled_(false),
      process_(base::kNullProcessHandle),
      process_id_(-1),
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      enable_file_cookies_(true),
      profile_type_(UITestBase::DEFAULT_THEME),
      shutdown_type_(UITestBase::WINDOW_CLOSE),
      test_start_time_(Time::NowFromSystemTime()),
      temp_profile_dir_(new ScopedTempDir()) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
}

UITestBase::UITestBase(MessageLoop::Type msg_loop_type)
    : launch_arguments_(CommandLine::ARGUMENTS_ONLY),
      expected_errors_(0),
      expected_crashes_(0),
      homepage_(chrome::kAboutBlankURL),
      wait_for_initial_loads_(true),
      dom_automation_enabled_(false),
      process_(base::kNullProcessHandle),
      process_id_(-1),
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      enable_file_cookies_(true),
      profile_type_(UITestBase::DEFAULT_THEME),
      shutdown_type_(UITestBase::WINDOW_CLOSE),
      test_start_time_(Time::NowFromSystemTime()) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
}

UITestBase::~UITestBase() {
}

void UITestBase::SetUp() {
  AssertAppNotRunning(L"Please close any other instances "
                      L"of the app before testing.");

  JavaScriptExecutionController::set_timeout(
      TestTimeouts::action_max_timeout_ms());
  LaunchBrowserAndServer();
}

void UITestBase::TearDown() {
  CloseBrowserAndServer();

  // Make sure that we didn't encounter any assertion failures
  logging::AssertionList assertions;
  logging::GetFatalAssertions(&assertions);

  // If there were errors, get all the error strings for display.
  std::wstring failures =
    L"The following error(s) occurred in the application during this test:";
  if (assertions.size() > expected_errors_) {
    logging::AssertionList::const_iterator iter = assertions.begin();
    for (; iter != assertions.end(); ++iter) {
      failures.append(L"\n\n");
      failures.append(*iter);
    }
  }
  EXPECT_EQ(expected_errors_, assertions.size()) << failures;

  int actual_crashes = GetCrashCount();

  std::wstring error_msg =
      L"Encountered an unexpected crash in the program during this test.";
  if (expected_crashes_ > 0 && actual_crashes == 0) {
    error_msg += L"  ";
    error_msg += kFailedNoCrashService;
  }
  EXPECT_EQ(expected_crashes_, actual_crashes) << error_msg;
}

// TODO(phajdan.jr): get rid of set_command_execution_timeout_ms.
void UITestBase::set_command_execution_timeout_ms(int timeout) {
  server_->set_command_execution_timeout_ms(timeout);
  LOG(INFO) << "Automation command execution timeout set to "
            << timeout << " milli secs.";
}

AutomationProxy* UITestBase::CreateAutomationProxy(int execution_timeout) {
  return new AutomationProxy(execution_timeout, false);
}

void UITestBase::LaunchBrowserAndServer() {
  // Set up IPC testing interface server.
  server_.reset(CreateAutomationProxy(
                    TestTimeouts::command_execution_timeout_ms()));

  LaunchBrowser(launch_arguments_, clear_profile_);
  server_->WaitForAppLaunch();
  if (wait_for_initial_loads_)
    ASSERT_TRUE(server_->WaitForInitialLoads());
  else
    PlatformThread::Sleep(sleep_timeout_ms());

  EXPECT_TRUE(automation()->SetFilteredInet(ShouldFilterInet()));
}

void UITestBase::CloseBrowserAndServer() {
  QuitBrowser();
  CleanupAppProcesses();

  // Suppress spammy failures that seem to be occurring when running
  // the UI tests in single-process mode.
  // TODO(jhughes): figure out why this is necessary at all, and fix it
  if (!in_process_renderer_)
    AssertAppNotRunning(StringPrintf(
        L"Unable to quit all browser processes. Original PID %d", process_id_));

  server_.reset();  // Shut down IPC testing interface.
}

static CommandLine* CreatePythonCommandLine() {
  return new CommandLine(FilePath(FILE_PATH_LITERAL("python")));
}

static CommandLine* CreateHttpServerCommandLine() {
  FilePath src_path;
  // Get to 'src' dir.
  PathService::Get(base::DIR_SOURCE_ROOT, &src_path);

  FilePath script_path(src_path);
  script_path = script_path.AppendASCII("third_party");
  script_path = script_path.AppendASCII("WebKit");
  script_path = script_path.AppendASCII("WebKitTools");
  script_path = script_path.AppendASCII("Scripts");
  script_path = script_path.AppendASCII("new-run-webkit-httpd");

  CommandLine* cmd_line = CreatePythonCommandLine();
  cmd_line->AppendArgPath(script_path);
  return cmd_line;
}

static void RunCommand(const CommandLine& cmd_line) {
#if defined(OS_WIN)
  // For Win32, use this 'version' of base::LaunchApp() with bInheritHandles
  // parameter to CreateProcess set to TRUE. This is needed in test harness
  // because it launches all the processes with 'chained' standard i/o pipes.
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_info;
  if (!CreateProcess(
           NULL,
           const_cast<wchar_t*>(cmd_line.command_line_string().c_str()),
           NULL, NULL,
           TRUE,  // Inherit the standard pipes, needed when
                  // running in test harnesses.
           0, NULL, NULL, &startup_info, &process_info))
    return;

  // Handles must be closed or they will leak
  CloseHandle(process_info.hThread);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  CloseHandle(process_info.hProcess);
#else
  base::LaunchApp(cmd_line, true, false, NULL);
#endif
}

void UITestBase::StartHttpServer(const FilePath& root_directory) {
  StartHttpServerWithPort(root_directory, 0);
}

void UITestBase::StartHttpServerWithPort(const FilePath& root_directory,
                                         int port) {
  scoped_ptr<CommandLine> cmd_line(CreateHttpServerCommandLine());
  ASSERT_TRUE(cmd_line.get());
  cmd_line->AppendSwitchASCII("server", "start");
  cmd_line->AppendSwitch("register_cygwin");
  cmd_line->AppendSwitchPath("root", root_directory);

  // For Windows 7, if we start the lighttpd server on the foreground mode,
  // it will mess up with the command window and cause conhost.exe to crash. To
  // work around this, we start the http server on the background mode.
#if defined(OS_WIN)
  if (win_util::GetWinVersion() >= win_util::WINVERSION_WIN7)
    cmd_line->AppendSwitch("run_background");
#endif

  if (port)
    cmd_line->AppendSwitchASCII("port", base::IntToString(port));
  RunCommand(*cmd_line.get());
}

void UITestBase::StopHttpServer() {
  scoped_ptr<CommandLine> cmd_line(CreateHttpServerCommandLine());
  ASSERT_TRUE(cmd_line.get());
  cmd_line->AppendSwitchASCII("server", "stop");
  RunCommand(*cmd_line.get());
}

void UITestBase::LaunchBrowser(const CommandLine& arguments,
                               bool clear_profile) {
  if (clear_profile || !temp_profile_dir_->IsValid()) {
    temp_profile_dir_.reset(new ScopedTempDir());
    ASSERT_TRUE(temp_profile_dir_->CreateUniqueTempDir());

    ASSERT_TRUE(
        test_launcher_utils::OverrideUserDataDir(temp_profile_dir_->path()));
  }

  if (!template_user_data_.empty()) {
    // Recursively copy the template directory to the user_data_dir.
    ASSERT_TRUE(file_util::CopyRecursiveDirNoCache(
        template_user_data_,
        user_data_dir()));
    // If we're using the complex theme data, we need to write the
    // user_data_dir_ to our preferences file.
    if (profile_type_ == UITestBase::COMPLEX_THEME) {
      RewritePreferencesFile(user_data_dir());
    }

    // Update the history file to include recent dates.
    UpdateHistoryDates();
  }

  ASSERT_TRUE(LaunchBrowserHelper(arguments, false, &process_));
  process_id_ = base::GetProcId(process_);
}

#if !defined(OS_MACOSX)
bool UITestBase::LaunchAnotherBrowserBlockUntilClosed(
    const CommandLine& cmdline) {
  return LaunchBrowserHelper(cmdline, true, NULL);
}
#endif

void UITestBase::QuitBrowser() {
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
    TimeTicks quit_start = TimeTicks::Now();
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
    if (!WaitForBrowserProcessToQuit()) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
    browser_quit_time_ = TimeTicks::Now() - quit_start;
  }

  // Don't forget to close the handle
  base::CloseProcessHandle(process_);
  process_ = base::kNullProcessHandle;
  process_id_ = -1;
}

void UITestBase::TerminateBrowser() {
  if (IsBrowserRunning()) {
    TimeTicks quit_start = TimeTicks::Now();
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

    if (!WaitForBrowserProcessToQuit()) {
      // We need to force the browser to quit because it didn't quit fast
      // enough. Take no chance and kill every chrome processes.
      CleanupAppProcesses();
    }
    browser_quit_time_ = TimeTicks::Now() - quit_start;
  }

  // Don't forget to close the handle
  base::CloseProcessHandle(process_);
  process_ = base::kNullProcessHandle;
  process_id_ = -1;
}

void UITestBase::AssertAppNotRunning(const std::wstring& error_message) {
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

void UITestBase::CleanupAppProcesses() {
  TerminateAllChromeProcesses(process_id_);
}

scoped_refptr<TabProxy> UITestBase::GetActiveTab(int window_index) {
  EXPECT_GE(window_index, 0);
  int window_count = -1;
  // We have to use EXPECT rather than ASSERT here because ASSERT_* only works
  // in functions that return void.
  EXPECT_TRUE(automation()->GetBrowserWindowCount(&window_count));
  if (window_count == -1)
    return NULL;
  EXPECT_GT(window_count, window_index);
  scoped_refptr<BrowserProxy> window_proxy(automation()->
      GetBrowserWindow(window_index));
  EXPECT_TRUE(window_proxy.get());
  if (!window_proxy.get())
    return NULL;

  int active_tab_index = -1;
  EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
  if (active_tab_index == -1)
    return NULL;

  return window_proxy->GetTab(active_tab_index);
}

scoped_refptr<TabProxy> UITestBase::GetActiveTab() {
  scoped_refptr<BrowserProxy> window_proxy(automation()->
      GetBrowserWindow(0));
  EXPECT_TRUE(window_proxy.get());
  if (!window_proxy.get())
    return NULL;

  scoped_refptr<TabProxy> tab_proxy = window_proxy->GetActiveTab();
  EXPECT_TRUE(tab_proxy.get());
  return tab_proxy;
}

void UITestBase::NavigateToURLAsync(const GURL& url) {
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  ASSERT_TRUE(tab_proxy->NavigateToURLAsync(url));
}

void UITestBase::NavigateToURL(const GURL& url) {
  NavigateToURL(url, 0, GetActiveTabIndex(0));
}

void UITestBase::NavigateToURL(const GURL& url, int window_index, int
    tab_index) {
  NavigateToURLBlockUntilNavigationsComplete(url, 1, window_index, tab_index);
}

void UITestBase::NavigateToURLBlockUntilNavigationsComplete(
    const GURL& url, int number_of_navigations) {
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab_proxy->NavigateToURLBlockUntilNavigationsComplete(
                url, number_of_navigations)) << url.spec();
}

void UITestBase::NavigateToURLBlockUntilNavigationsComplete(
    const GURL& url, int number_of_navigations, int window_index,
    int tab_index) {
  scoped_refptr<BrowserProxy> window =
    automation()->GetBrowserWindow(window_index);
  ASSERT_TRUE(window.get());
  scoped_refptr<TabProxy> tab_proxy(window->GetTab(tab_index));
  ASSERT_TRUE(tab_proxy.get());
  EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
            tab_proxy->NavigateToURLBlockUntilNavigationsComplete(
                url, number_of_navigations)) << url.spec();
}

bool UITestBase::WaitForDownloadShelfVisible(BrowserProxy* browser) {
  return WaitForDownloadShelfVisibilityChange(browser, true);
}

bool UITestBase::WaitForDownloadShelfInvisible(BrowserProxy* browser) {
  return WaitForDownloadShelfVisibilityChange(browser, false);
}

bool UITestBase::WaitForBrowserProcessToQuit() {
  // Wait for the browser process to quit.
  int timeout = TestTimeouts::wait_for_terminate_timeout_ms();
#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  timeout = 500000;
#endif
  return base::WaitForSingleProcess(process_, timeout);
}

bool UITestBase::WaitForDownloadShelfVisibilityChange(BrowserProxy* browser,
                                                      bool wait_for_open) {
  const int kCycles = 10;
  for (int i = 0; i < kCycles; i++) {
    // Give it a chance to catch up.
    bool browser_survived = CrashAwareSleep(sleep_timeout_ms() / kCycles);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return false;

    bool visible = !wait_for_open;
    if (!browser->IsShelfVisible(&visible))
      continue;
    if (visible == wait_for_open)
      return true;  // Got the download shelf.
  }

  ADD_FAILURE() << "Timeout reached in WaitForDownloadShelfVisibilityChange";
  return false;
}

bool UITestBase::WaitForFindWindowVisibilityChange(BrowserProxy* browser,
                                                   bool wait_for_open) {
  const int kCycles = 10;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    if (!browser->IsFindWindowFullyVisible(&visible))
      return false;  // Some error.
    if (visible == wait_for_open)
      return true;  // Find window visibility change complete.

    // Give it a chance to catch up.
    bool browser_survived = CrashAwareSleep(sleep_timeout_ms() / kCycles);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return false;
  }

  ADD_FAILURE() << "Timeout reached in WaitForFindWindowVisibilityChange";
  return false;
}

bool UITestBase::WaitForBookmarkBarVisibilityChange(BrowserProxy* browser,
                                                    bool wait_for_open) {
  const int kCycles = 10;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    bool animating = true;
    if (!browser->GetBookmarkBarVisibility(&visible, &animating))
      return false;  // Some error.
    if (visible == wait_for_open && !animating)
      return true;  // Bookmark bar visibility change complete.

    // Give it a chance to catch up.
    bool browser_survived = CrashAwareSleep(sleep_timeout_ms() / kCycles);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return false;
  }

  ADD_FAILURE() << "Timeout reached in WaitForBookmarkBarVisibilityChange";
  return false;
}

GURL UITestBase::GetActiveTabURL(int window_index) {
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab(window_index));
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return GURL();

  GURL url;
  bool success = tab_proxy->GetCurrentURL(&url);
  EXPECT_TRUE(success);
  if (!success)
    return GURL();
  return url;
}

std::wstring UITestBase::GetActiveTabTitle(int window_index) {
  std::wstring title;
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab(window_index));
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return title;

  EXPECT_TRUE(tab_proxy->GetTabTitle(&title));
  return title;
}

int UITestBase::GetActiveTabIndex(int window_index) {
  scoped_refptr<BrowserProxy> window_proxy(automation()->
      GetBrowserWindow(window_index));
  EXPECT_TRUE(window_proxy.get());
  if (!window_proxy.get())
    return -1;

  int active_tab_index = -1;
  EXPECT_TRUE(window_proxy->GetActiveTabIndex(&active_tab_index));
  return active_tab_index;
}

bool UITestBase::IsBrowserRunning() {
  return CrashAwareSleep(0);
}

bool UITestBase::CrashAwareSleep(int time_out_ms) {
  return base::CrashAwareSleep(process_, time_out_ms);
}

int UITestBase::GetBrowserProcessCount() {
  return GetRunningChromeProcesses(process_id_).size();
}

int UITestBase::GetCrashCount() {
  FilePath crash_dump_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  int actual_crashes = file_util::CountFilesCreatedAfter(
      crash_dump_path, test_start_time_);

#if defined(OS_WIN)
  // Each crash creates two dump files, so we divide by two here.
  actual_crashes /= 2;
#endif

  return actual_crashes;
}

static DictionaryValue* LoadDictionaryValueFromPath(const FilePath& path) {
  if (path.empty())
    return NULL;

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> root_value(serializer.Deserialize(NULL, NULL));
  if (!root_value.get() || root_value->GetType() != Value::TYPE_DICTIONARY)
    return NULL;

  return static_cast<DictionaryValue*>(root_value.release());
}

DictionaryValue* UITestBase::GetLocalState() {
  FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  return LoadDictionaryValueFromPath(local_state_path);
}

DictionaryValue* UITestBase::GetDefaultProfilePreferences() {
  FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(WideToUTF8(chrome::kNotSignedInProfile));
  return LoadDictionaryValueFromPath(path.Append(chrome::kPreferencesFilename));
}

int UITestBase::GetTabCount() {
  return GetTabCount(0);
}

int UITestBase::GetTabCount(int window_index) {
  scoped_refptr<BrowserProxy> window(
      automation()->GetBrowserWindow(window_index));
  EXPECT_TRUE(window.get());
  if (!window.get())
    return 0;

  int result = 0;
  EXPECT_TRUE(window->GetTabCount(&result));

  return result;
}

bool UITestBase::WaitUntilCookieValue(TabProxy* tab,
                                      const GURL& url,
                                      const char* cookie_name,
                                      int time_out_ms,
                                      const char* expected_value) {
  const int kIntervalMs = 250;
  const int kMaxIntervals = time_out_ms / kIntervalMs;

  std::string cookie_value;
  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(kIntervalMs);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return false;

    EXPECT_TRUE(tab->GetCookieByName(url, cookie_name, &cookie_value));
    if (cookie_value == expected_value)
      return true;
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilCookieValue";
  return false;
}

std::string UITestBase::WaitUntilCookieNonEmpty(TabProxy* tab,
                                                const GURL& url,
                                                const char* cookie_name,
                                                int time_out_ms) {
  const int kIntervalMs = 250;
  const int kMaxIntervals = time_out_ms / kIntervalMs;

  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(kIntervalMs);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return std::string();

    std::string cookie_value;
    EXPECT_TRUE(tab->GetCookieByName(url, cookie_name, &cookie_value));
    if (!cookie_value.empty())
      return cookie_value;
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilCookieNonEmpty";
  return std::string();
}

bool UITestBase::WaitUntilJavaScriptCondition(TabProxy* tab,
                                              const std::wstring& frame_xpath,
                                              const std::wstring& jscript,
                                              int time_out_ms) {
  const int kIntervalMs = 250;
  const int kMaxIntervals = time_out_ms / kIntervalMs;

  // Wait until the test signals it has completed.
  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(kIntervalMs);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return false;

    bool done_value = false;
    bool success = tab->ExecuteAndExtractBool(frame_xpath, jscript,
                                              &done_value);
    EXPECT_TRUE(success);
    if (!success)
      return false;
    if (done_value)
      return true;
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilJavaScriptCondition";
  return false;
}

void UITestBase::WaitUntilTabCount(int tab_count) {
  const int kMaxIntervals = 10;
  const int kIntervalMs = sleep_timeout_ms() / kMaxIntervals;

  for (int i = 0; i < kMaxIntervals; ++i) {
    bool browser_survived = CrashAwareSleep(kIntervalMs);
    EXPECT_TRUE(browser_survived);
    if (!browser_survived)
      return;
    if (GetTabCount() == tab_count)
      return;
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilTabCount";
}

FilePath UITestBase::GetDownloadDirectory() {
  scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
  EXPECT_TRUE(tab_proxy.get());
  if (!tab_proxy.get())
    return FilePath();

  FilePath download_directory;
  EXPECT_TRUE(tab_proxy->GetDownloadDirectory(&download_directory));
  return download_directory;
}

void UITestBase::CloseBrowserAsync(BrowserProxy* browser) const {
  ASSERT_TRUE(server_->Send(
      new AutomationMsg_CloseBrowserRequestAsync(0, browser->handle())));
}

bool UITestBase::CloseBrowser(BrowserProxy* browser,
                              bool* application_closed) const {
  DCHECK(application_closed);
  if (!browser->is_valid() || !browser->handle())
    return false;

  bool result = true;

  bool succeeded = server_->Send(new AutomationMsg_CloseBrowser(
      0, browser->handle(), &result, application_closed));

  if (!succeeded)
    return false;

  if (*application_closed) {
    // Let's wait until the process dies (if it is not gone already).
    bool success = base::WaitForSingleProcess(process_, base::kNoTimeout);
    EXPECT_TRUE(success);
  }

  return result;
}

void UITestBase::WaitForFinish(const std::string &name,
                           const std::string &id,
                           const GURL &url,
                           const std::string& test_complete_cookie,
                           const std::string& expected_cookie_value,
                           const int wait_time) {
  // The webpage being tested has javascript which sets a cookie
  // which signals completion of the test.  The cookie name is
  // a concatenation of the test name and the test id.  This allows
  // us to run multiple tests within a single webpage and test
  // that they all c
  std::string cookie_name = name;
  cookie_name.append(".");
  cookie_name.append(id);
  cookie_name.append(".");
  cookie_name.append(test_complete_cookie);

  scoped_refptr<TabProxy> tab(GetActiveTab());
  ASSERT_TRUE(tab.get());
  std::string cookie_value = WaitUntilCookieNonEmpty(tab.get(), url,
                                                     cookie_name.c_str(),
                                                     wait_time);
  EXPECT_EQ(expected_cookie_value, cookie_value);
}

bool UITestBase::EvictFileFromSystemCacheWrapper(const FilePath& path) {
  for (int i = 0; i < 10; i++) {
    if (file_util::EvictFileFromSystemCache(path))
      return true;
    PlatformThread::Sleep(sleep_timeout_ms() / 10);
  }
  return false;
}

// static
void UITestBase::RewritePreferencesFile(const FilePath& user_data_dir) {
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

FilePath UITestBase::user_data_dir() const {
  EXPECT_TRUE(temp_profile_dir_->IsValid());
  return temp_profile_dir_->path();
}

// static
FilePath UITestBase::ComputeTypicalUserDataSource(ProfileType profile_type) {
  FilePath source_history_file;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                               &source_history_file));
  source_history_file = source_history_file.AppendASCII("profiles");
  switch (profile_type) {
    case UITestBase::DEFAULT_THEME:
      source_history_file = source_history_file.AppendASCII("typical_history");
      break;
    case UITestBase::COMPLEX_THEME:
      source_history_file = source_history_file.AppendASCII("complex_theme");
      break;
    case UITestBase::NATIVE_THEME:
      source_history_file = source_history_file.AppendASCII("gtk_theme");
      break;
    case UITestBase::CUSTOM_FRAME:
      source_history_file = source_history_file.AppendASCII("custom_frame");
      break;
    case UITestBase::CUSTOM_FRAME_NATIVE_THEME:
      source_history_file =
          source_history_file.AppendASCII("custom_frame_gtk_theme");
      break;
    default:
      NOTREACHED();
  }
  return source_history_file;
}

void UITestBase::WaitForGeneratedFileAndCheck(
    const FilePath& generated_file,
    const FilePath& original_file,
    bool compare_files,
    bool need_equal,
    bool delete_generated_file) {
  // Check whether the target file has been generated.
  base::PlatformFileInfo previous, current;
  bool exist = false;
  const int kCycles = 20;
  for (int i = 0; i < kCycles; ++i) {
    if (exist) {
      file_util::GetFileInfo(generated_file, &current);
      if (current.size == previous.size)
        break;
      previous = current;
    } else if (file_util::PathExists(generated_file)) {
      file_util::GetFileInfo(generated_file, &previous);
      exist = true;
    }
    PlatformThread::Sleep(sleep_timeout_ms() / kCycles);
  }
  EXPECT_TRUE(exist);

  if (compare_files) {
    // Check whether the generated file is equal with original file according to
    // parameter: need_equal.
    int64 generated_file_size = 0;
    int64 original_file_size = 0;

    EXPECT_TRUE(file_util::GetFileSize(generated_file, &generated_file_size));
    EXPECT_TRUE(file_util::GetFileSize(original_file, &original_file_size));
    if (need_equal) {
      EXPECT_EQ(generated_file_size, original_file_size);
      EXPECT_TRUE(file_util::ContentsEqual(generated_file, original_file));
    } else {
      EXPECT_NE(generated_file_size, original_file_size);
      EXPECT_FALSE(file_util::ContentsEqual(generated_file, original_file));
    }
  }
  if (delete_generated_file)
    EXPECT_TRUE(file_util::DieFileDie(generated_file, false));
}

bool UITestBase::LaunchBrowserHelper(const CommandLine& arguments,
                                     bool wait,
                                     base::ProcessHandle* process) {
  FilePath command = browser_directory_.Append(
      FilePath::FromWStringHack(chrome::kBrowserProcessExecutablePath));
  CommandLine command_line(command);

  // Add any explicit command line flags passed to the process.
  CommandLine::StringType extra_chrome_flags =
      CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kExtraChromeFlags);
  if (!extra_chrome_flags.empty()) {
    // Split by spaces and append to command line
    std::vector<CommandLine::StringType> flags;
    SplitString(extra_chrome_flags, ' ', &flags);
    for (size_t i = 0; i < flags.size(); ++i)
      command_line.AppendArgNative(flags[i]);
  }

  // No first-run dialogs, please.
  command_line.AppendSwitch(switches::kNoFirstRun);

  // No default browser check, it would create an info-bar (if we are not the
  // default browser) that could conflicts with some tests expectations.
  command_line.AppendSwitch(switches::kNoDefaultBrowserCheck);

  // This is a UI test.
  command_line.AppendSwitchASCII(switches::kTestType, kUITestType);

  // Tell the browser to use a temporary directory just for this test.
  command_line.AppendSwitchPath(switches::kUserDataDir, user_data_dir());

  // We need cookies on file:// for things like the page cycler.
  if (enable_file_cookies_)
    command_line.AppendSwitch(switches::kEnableFileCookies);

  if (dom_automation_enabled_)
    command_line.AppendSwitch(switches::kDomAutomationController);

  if (include_testing_id_) {
    command_line.AppendSwitchASCII(switches::kTestingChannelID,
                                   server_->channel_id());
  }

  if (!show_error_dialogs_ &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableErrorDialogs)) {
    command_line.AppendSwitch(switches::kNoErrorDialogs);
  }
  if (in_process_renderer_)
    command_line.AppendSwitch(switches::kSingleProcess);
  if (no_sandbox_)
    command_line.AppendSwitch(switches::kNoSandbox);
  if (full_memory_dump_)
    command_line.AppendSwitch(switches::kFullMemoryCrashReport);
  if (safe_plugins_)
    command_line.AppendSwitch(switches::kSafePlugins);
  if (enable_dcheck_)
    command_line.AppendSwitch(switches::kEnableDCHECK);
  if (silent_dump_on_dcheck_)
    command_line.AppendSwitch(switches::kSilentDumpOnDCHECK);
  if (disable_breakpad_)
    command_line.AppendSwitch(switches::kDisableBreakpad);
  if (!homepage_.empty())
    command_line.AppendSwitchASCII(switches::kHomePage, homepage_);
  // Don't try to fetch web resources during UI testing.
  command_line.AppendSwitch(switches::kDisableWebResources);

  if (!js_flags_.empty())
    command_line.AppendSwitchASCII(switches::kJavaScriptFlags, js_flags_);
  if (!log_level_.empty())
    command_line.AppendSwitchASCII(switches::kLoggingLevel, log_level_);

  command_line.AppendSwitch(switches::kMetricsRecordingOnly);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableErrorDialogs))
    command_line.AppendSwitch(switches::kEnableLogging);

  if (dump_histograms_on_exit_)
    command_line.AppendSwitch(switches::kDumpHistogramsOnExit);

#ifdef WAIT_FOR_DEBUGGER_ON_OPEN
  command_line.AppendSwitch(switches::kDebugOnStart);
#endif

  if (!ui_test_name_.empty())
    command_line.AppendSwitchASCII(switches::kTestName, ui_test_name_);

  // The tests assume that file:// URIs can freely access other file:// URIs.
  command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);

  // Disable TabCloseableStateWatcher for tests.
  command_line.AppendSwitch(switches::kDisableTabCloseableStateWatcher);

  DebugFlags::ProcessDebugFlags(
      &command_line, ChildProcessInfo::UNKNOWN_PROCESS, false);
  command_line.AppendArguments(arguments, false);

  // TODO(phajdan.jr): Only run it for "main" browser launch.
  browser_launch_time_ = TimeTicks::Now();

#if defined(OS_WIN)
  bool started = base::LaunchApp(command_line,
                                 wait,
                                 !show_window_,
                                 process);
#elif defined(OS_POSIX)
  // Sometimes one needs to run the browser under a special environment
  // (e.g. valgrind) without also running the test harness (e.g. python)
  // under the special environment.  Provide a way to wrap the browser
  // commandline with a special prefix to invoke the special environment.
  const char* browser_wrapper = getenv("BROWSER_WRAPPER");
  if (browser_wrapper) {
    command_line.PrependWrapper(browser_wrapper);
    LOG(INFO) << "BROWSER_WRAPPER was set, prefixing command_line with "
              << browser_wrapper;
  }

  bool started = base::LaunchApp(command_line.argv(),
                                 server_->fds_to_map(),
                                 wait,
                                 process);
#endif

  return started;
}

void UITestBase::UpdateHistoryDates() {
  // Migrate the times in the segment_usage table to yesterday so we get
  // actual thumbnails on the NTP.
  sql::Connection db;
  FilePath history =
      user_data_dir().AppendASCII("Default").AppendASCII("History");
  // Not all test profiles have a history file.
  if (!file_util::PathExists(history))
    return;

  ASSERT_TRUE(db.Open(history));
  Time yesterday = Time::Now() - TimeDelta::FromDays(1);
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

void UITestBase::SetBrowserDirectory(const FilePath& dir) {
  browser_directory_ = dir;
}

// UITest methods

void UITest::SetUp() {
  // Pass the test case name to chrome.exe on the command line to help with
  // parsing Purify output.
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  if (test_info) {
    set_ui_test_name(test_info->test_case_name() + std::string(".") +
                     test_info->name());
  }
  UITestBase::SetUp();
  PlatformTest::SetUp();
}

void UITest::TearDown() {
  UITestBase::TearDown();
  PlatformTest::TearDown();
}

AutomationProxy* UITest::CreateAutomationProxy(int execution_timeout) {
  // Make the AutomationProxy disconnect the channel on the first error,
  // so that we avoid spending a lot of time in timeouts. The browser is likely
  // hosed if we hit those errors.
  return new AutomationProxy(execution_timeout, true);
}
