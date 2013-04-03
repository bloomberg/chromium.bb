// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/ui_test.h"

#if defined(OS_POSIX)
#include <signal.h>
#include <sys/types.h>
#endif

#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/test_file_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "ui/gl/gl_implementation.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(USE_AURA)
#include "ui/compositor/compositor_switches.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

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

UITestBase::UITestBase()
    : launch_arguments_(CommandLine::NO_PROGRAM),
      expected_errors_(0),
      expected_crashes_(0),
      homepage_(chrome::kAboutBlankURL),
      wait_for_initial_loads_(true),
      dom_automation_enabled_(false),
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      enable_file_cookies_(true),
      profile_type_(UITestBase::DEFAULT_THEME) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
}

UITestBase::UITestBase(MessageLoop::Type msg_loop_type)
    : launch_arguments_(CommandLine::NO_PROGRAM),
      expected_errors_(0),
      expected_crashes_(0),
      wait_for_initial_loads_(true),
      dom_automation_enabled_(false),
      show_window_(false),
      clear_profile_(true),
      include_testing_id_(true),
      enable_file_cookies_(true),
      profile_type_(UITestBase::DEFAULT_THEME) {
  PathService::Get(chrome::DIR_APP, &browser_directory_);
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_directory_);
}

UITestBase::~UITestBase() {}

void UITestBase::SetUp() {
  // Tests that do a session restore (e.g. SessionRestoreUITest, StartupTest)
  // call SetUp() multiple times because they restart the browser mid-test.
  // We don't want to reset the ProxyLauncher's state in those cases.
  if (!launcher_.get())
    launcher_.reset(CreateProxyLauncher());
  launcher_->AssertAppNotRunning("Please close any other instances "
                                 "of the app before testing.");

  test_start_time_ = Time::NowFromSystemTime();

  SetLaunchSwitches();
  ASSERT_TRUE(launcher_->InitializeConnection(DefaultLaunchState(),
                                              wait_for_initial_loads_));
}

void UITestBase::TearDown() {
  if (launcher_.get())
    launcher_->TerminateConnection();

  CheckErrorsAndCrashes();
}

AutomationProxy* UITestBase::automation() const {
  return launcher_->automation();
}

base::TimeDelta UITestBase::action_timeout() {
  return automation()->action_timeout();
}

int UITestBase::action_timeout_ms() {
  return action_timeout().InMilliseconds();
}

void UITestBase::set_action_timeout(base::TimeDelta timeout) {
  automation()->set_action_timeout(timeout);
  VLOG(1) << "Automation action timeout set to "
          << timeout.InMilliseconds() << " ms";
}

void UITestBase::set_action_timeout_ms(int timeout) {
  set_action_timeout(base::TimeDelta::FromMilliseconds(timeout));
}

ProxyLauncher* UITestBase::CreateProxyLauncher() {
  return new AnonymousProxyLauncher(false);
}

ProxyLauncher::LaunchState UITestBase::DefaultLaunchState() {
  base::FilePath browser_executable =
      browser_directory_.Append(GetExecutablePath());
  CommandLine command(browser_executable);
  command.AppendArguments(launch_arguments_, false);
  base::Closure setup_profile_callback = base::Bind(&UITestBase::SetUpProfile,
                                                    base::Unretained(this));
  ProxyLauncher::LaunchState state =
      { clear_profile_, template_user_data_, setup_profile_callback,
        command, include_testing_id_, show_window_ };
  return state;
}

void UITestBase::SetLaunchSwitches() {
  // All flags added here should also be added in ExtraChromeFlags() in
  // chrome/test/pyautolib/pyauto.py as well to take effect for all tests
  // on chromeos.

  // Propagate commandline settings from test_launcher_utils.
  test_launcher_utils::PrepareBrowserCommandLineForTests(&launch_arguments_);

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kWaitForDebugger))
    launch_arguments_.AppendSwitch(switches::kWaitForDebugger);

  // We need cookies on file:// for things like the page cycler.
  if (enable_file_cookies_)
    launch_arguments_.AppendSwitch(switches::kEnableFileCookies);
  if (dom_automation_enabled_)
    launch_arguments_.AppendSwitch(switches::kDomAutomationController);
  // Allow off-store extension installs.
  launch_arguments_.AppendSwitchASCII(
      switches::kEasyOffStoreExtensionInstall, "1");
  if (!homepage_.empty()) {
    // Pass |homepage_| both as an arg (so that it opens on startup) and to the
    // homepage switch (so that the homepage is set).

    if (!launch_arguments_.HasSwitch(switches::kHomePage))
      launch_arguments_.AppendSwitchASCII(switches::kHomePage, homepage_);

    if (launch_arguments_.GetArgs().empty() &&
        !launch_arguments_.HasSwitch(switches::kRestoreLastSession)) {
      launch_arguments_.AppendArg(homepage_);
    }
  }
  if (!test_name_.empty())
    launch_arguments_.AppendSwitchASCII(switches::kTestName, test_name_);
#if defined(USE_AURA)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableTestCompositor)) {
    launch_arguments_.AppendSwitch(switches::kTestCompositor);
  }
#endif
}

void UITestBase::SetUpProfile() {
}

void UITestBase::LaunchBrowser() {
  LaunchBrowser(launch_arguments_, clear_profile_);
}

void UITestBase::LaunchBrowserAndServer() {
  ASSERT_TRUE(launcher_->LaunchBrowserAndServer(DefaultLaunchState(),
                                                wait_for_initial_loads_));
}

void UITestBase::ConnectToRunningBrowser() {
  ASSERT_TRUE(launcher_->ConnectToRunningBrowser(wait_for_initial_loads_));
}

void UITestBase::CloseBrowserAndServer() {
  if (launcher_.get())
    launcher_->CloseBrowserAndServer();
}

void UITestBase::LaunchBrowser(const CommandLine& arguments,
                               bool clear_profile) {
  ProxyLauncher::LaunchState state = DefaultLaunchState();
  state.clear_profile = clear_profile;
  ASSERT_TRUE(launcher_->LaunchBrowser(state));
}

void UITestBase::QuitBrowser() {
  launcher_->QuitBrowser();
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

void UITestBase::NavigateToURL(const GURL& url) {
  NavigateToURL(url, 0, GetActiveTabIndex(0));
}

void UITestBase::NavigateToURL(const GURL& url, int window_index) {
  NavigateToURL(url, window_index, GetActiveTabIndex(window_index));
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

void UITestBase::WaitUntilTabCount(int tab_count) {
  const int kMaxIntervals = 10;
  const TimeDelta kDelay = TestTimeouts::action_timeout() / kMaxIntervals;

  for (int i = 0; i < kMaxIntervals; ++i) {
    if (GetTabCount() == tab_count)
      return;

    base::PlatformThread::Sleep(kDelay);
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilTabCount";
}

const base::FilePath::CharType* UITestBase::GetExecutablePath() {
  if (launch_arguments_.HasSwitch(switches::kEnableChromiumBranding))
    return chrome::kBrowserProcessExecutablePathChromium;
  return chrome::kBrowserProcessExecutablePath;
}

bool UITestBase::CloseBrowser(BrowserProxy* browser,
                              bool* application_closed) const {
  DCHECK(application_closed);
  if (!browser->is_valid() || !browser->handle())
    return false;

  bool result = true;

  bool succeeded = automation()->Send(new AutomationMsg_CloseBrowser(
      browser->handle(), &result, application_closed));

  if (!succeeded)
    return false;

  if (*application_closed) {
    int exit_code = -1;
    EXPECT_TRUE(launcher_->WaitForBrowserProcessToQuit(
        TestTimeouts::action_max_timeout(), &exit_code));
    EXPECT_EQ(0, exit_code);  // Expect a clean shutown.
  }

  return result;
}

// static
base::FilePath UITestBase::ComputeTypicalUserDataSource(
    UITestBase::ProfileType profile_type) {
  base::FilePath source_history_file;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA,
                               &source_history_file));
  source_history_file = source_history_file.AppendASCII("profiles");
  switch (profile_type) {
    case UITestBase::DEFAULT_THEME:
      source_history_file = source_history_file.AppendASCII(
          "profile_with_default_theme");
      break;
    case UITestBase::COMPLEX_THEME:
      source_history_file = source_history_file.AppendASCII(
          "profile_with_complex_theme");
      break;
    default:
      NOTREACHED();
  }

  return source_history_file;
}

int UITestBase::GetCrashCount() const {
  base::FilePath crash_dump_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);

  int files_found = 0;
  file_util::FileEnumerator en(crash_dump_path, false,
                               file_util::FileEnumerator::FILES);
  while (!en.Next().empty()) {
    file_util::FileEnumerator::FindInfo info;
    if (file_util::FileEnumerator::GetLastModifiedTime(info) > test_start_time_)
      files_found++;
  }

#if defined(OS_WIN)
  // Each crash creates two dump files on Windows.
  return files_found / 2;
#else
  return files_found;
 #endif
}

std::string UITestBase::CheckErrorsAndCrashes() const {
  // Make sure that we didn't encounter any assertion failures
  logging::AssertionList assertions;
  logging::GetFatalAssertions(&assertions);

  // If there were errors, get all the error strings for display.
  std::wstring failures =
      L"The following error(s) occurred in the application during this test:";
  if (assertions.size() > expected_errors_) {
    logging::AssertionList::const_iterator iter = assertions.begin();
    for (; iter != assertions.end(); ++iter) {
      failures += L"\n\n";
      failures += *iter;
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

  std::wstring wide_result;
  if (expected_errors_ != assertions.size()) {
    wide_result += failures;
    wide_result += L"\n\n";
  }
  if (expected_crashes_ != actual_crashes)
    wide_result += error_msg;

  return std::string(wide_result.begin(), wide_result.end());
}

void UITestBase::SetBrowserDirectory(const base::FilePath& dir) {
  browser_directory_ = dir;
}

void UITestBase::AppendBrowserLaunchSwitch(const char* name) {
  launch_arguments_.AppendSwitch(name);
}

void UITestBase::AppendBrowserLaunchSwitch(const char* name,
                                           const char* value) {
  launch_arguments_.AppendSwitchASCII(name, value);
}

bool UITestBase::BeginTracing(const std::string& categories) {
  return automation()->BeginTracing(categories);
}

std::string UITestBase::EndTracing() {
  std::string json_trace_output;
  if (!automation()->EndTracing(&json_trace_output))
    return "";
  return json_trace_output;
}

// UITest methods

void UITest::SetUp() {
  // Pass the test case name to chrome.exe on the command line to help with
  // parsing Purify output.
  const testing::TestInfo* const test_info =
      testing::UnitTest::GetInstance()->current_test_info();
  if (test_info) {
    set_test_name(test_info->test_case_name() + std::string(".") +
                  test_info->name());
  }

  UITestBase::SetUp();
  PlatformTest::SetUp();
}

void UITest::TearDown() {
  UITestBase::TearDown();
  PlatformTest::TearDown();
}

ProxyLauncher* UITest::CreateProxyLauncher() {
  // Make the AutomationProxy disconnect the channel on the first error,
  // so that we avoid spending a lot of time in timeouts. The browser is likely
  // hosed if we hit those errors.
  return new AnonymousProxyLauncher(true);
}

bool UITest::GetBrowserProcessCount(int* count) {
  *count = 0;
  if (!automation()->WaitForProcessLauncherThreadToGoIdle())
    return false;
  *count = GetRunningChromeProcesses(browser_process_id()).size();
  return true;
}

static DictionaryValue* LoadDictionaryValueFromPath(
    const base::FilePath& path) {
  if (path.empty())
    return NULL;

  JSONFileValueSerializer serializer(path);
  scoped_ptr<Value> root_value(serializer.Deserialize(NULL, NULL));
  if (!root_value.get() || root_value->GetType() != Value::TYPE_DICTIONARY)
    return NULL;

  return static_cast<DictionaryValue*>(root_value.release());
}

DictionaryValue* UITest::GetLocalState() {
  base::FilePath local_state_path;
  PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  return LoadDictionaryValueFromPath(local_state_path);
}

DictionaryValue* UITest::GetDefaultProfilePreferences() {
  base::FilePath path;
  PathService::Get(chrome::DIR_USER_DATA, &path);
  path = path.AppendASCII(TestingProfile::kTestUserProfileDir);
  return LoadDictionaryValueFromPath(path.Append(chrome::kPreferencesFilename));
}

void UITest::WaitForFinish(const std::string &name,
                           const std::string &id,
                           const GURL &url,
                           const std::string& test_complete_cookie,
                           const std::string& expected_cookie_value,
                           const base::TimeDelta wait_time) {
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

bool UITest::EvictFileFromSystemCacheWrapper(const base::FilePath& path) {
  const int kCycles = 10;
  const TimeDelta kDelay = TestTimeouts::action_timeout() / kCycles;
  for (int i = 0; i < kCycles; i++) {
    if (file_util::EvictFileFromSystemCache(path))
      return true;
    base::PlatformThread::Sleep(kDelay);
  }
  return false;
}

bool UITest::WaitUntilJavaScriptCondition(TabProxy* tab,
                                          const std::wstring& frame_xpath,
                                          const std::wstring& jscript,
                                          base::TimeDelta timeout) {
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(250);
  const int kMaxDelays = timeout / kDelay;

  // Wait until the test signals it has completed.
  for (int i = 0; i < kMaxDelays; ++i) {
    bool done_value = false;
    bool success = tab->ExecuteAndExtractBool(frame_xpath, jscript,
                                              &done_value);
    EXPECT_TRUE(success);
    if (!success)
      return false;
    if (done_value)
      return true;

    base::PlatformThread::Sleep(kDelay);
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilJavaScriptCondition";
  return false;
}

bool UITest::WaitUntilCookieValue(TabProxy* tab,
                                  const GURL& url,
                                  const char* cookie_name,
                                  base::TimeDelta timeout,
                                  const char* expected_value) {
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(250);
  const int kMaxDelays = timeout / kDelay;

  std::string cookie_value;
  for (int i = 0; i < kMaxDelays; ++i) {
    EXPECT_TRUE(tab->GetCookieByName(url, cookie_name, &cookie_value));
    if (cookie_value == expected_value)
      return true;

    base::PlatformThread::Sleep(kDelay);
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilCookieValue";
  return false;
}

std::string UITest::WaitUntilCookieNonEmpty(TabProxy* tab,
                                            const GURL& url,
                                            const char* cookie_name,
                                            base::TimeDelta timeout) {
  const TimeDelta kDelay = TimeDelta::FromMilliseconds(250);
  const int kMaxDelays = timeout / kDelay;

  for (int i = 0; i < kMaxDelays; ++i) {
    std::string cookie_value;
    EXPECT_TRUE(tab->GetCookieByName(url, cookie_name, &cookie_value));
    if (!cookie_value.empty())
      return cookie_value;

    base::PlatformThread::Sleep(kDelay);
  }

  ADD_FAILURE() << "Timeout reached in WaitUntilCookieNonEmpty";
  return std::string();
}

bool UITest::WaitForFindWindowVisibilityChange(BrowserProxy* browser,
                                               bool wait_for_open) {
  const int kCycles = 10;
  const TimeDelta kDelay = TestTimeouts::action_timeout() / kCycles;
  for (int i = 0; i < kCycles; i++) {
    bool visible = false;
    if (!browser->IsFindWindowFullyVisible(&visible))
      return false;  // Some error.
    if (visible == wait_for_open)
      return true;  // Find window visibility change complete.

    // Give it a chance to catch up.
    base::PlatformThread::Sleep(kDelay);
  }

  ADD_FAILURE() << "Timeout reached in WaitForFindWindowVisibilityChange";
  return false;
}

void UITest::TerminateBrowser() {
  launcher_->TerminateBrowser();

  // Make sure the UMA metrics say we didn't crash.
  scoped_ptr<DictionaryValue> local_prefs(GetLocalState());
  bool exited_cleanly;
  ASSERT_TRUE(local_prefs.get());
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilityExitedCleanly,
                                      &exited_cleanly));
  ASSERT_TRUE(exited_cleanly);

  // And that session end was successful.
  bool session_end_completed;
  ASSERT_TRUE(local_prefs->GetBoolean(prefs::kStabilitySessionEndCompleted,
                                      &session_end_completed));
  ASSERT_TRUE(session_end_completed);

  // Make sure session restore says we didn't crash.
  scoped_ptr<DictionaryValue> profile_prefs(GetDefaultProfilePreferences());
  ASSERT_TRUE(profile_prefs.get());
  std::string exit_type;
  ASSERT_TRUE(profile_prefs->GetString(prefs::kSessionExitedCleanly,
                                        &exit_type));
  EXPECT_EQ(ProfileImpl::kPrefExitTypeNormal, exit_type);
}
