// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides reliablity test which runs under UI test framework. The
// test is intended to run within QEMU environment.
//
// Usage 1: reliability_test
// Upon invocation, it visits a hard coded list of sample URLs. This is mainly
// used by buildbot, to verify reliability_test itself runs ok.
//
// Usage 2: reliability_test --site=url --startpage=start --endpage=end [...]
// Upon invocation, it visits a list of URLs constructed as
// "http://url/page?id=k". (start <= k <= end).
//
// Usage 3: reliability_test --list=file --startline=start --endline=end [...]
// Upon invocation, it visits each of the URLs on line numbers between start
// and end, inclusive, stored in the input file. The line number starts from 1.
//
// If both "--site" and "--list" are provided, the "--site" set of arguments
// are ignored.
//
// Optional Switches:
// --iterations=num: goes through the list of URLs constructed in usage 2 or 3
//                   num times.
// --continuousload: continuously visits the list of URLs without restarting
//                    browser for each page load.
// --memoryusage: prints out memory usage when visiting each page.
// --endurl=url: visits the specified url in the end.
// --logfile=filepath: saves the visit log to the specified path.
// --timeout=millisecond: time out as specified in millisecond during each
//                        page load.
// --nopagedown: won't simulate page down key presses after page load.
// --noclearprofile: do not clear profile dir before firing up each time.
// --savedebuglog: save Chrome, V8, and test debug log for each page loaded.

#include <fstream>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/reliability/page_load_test.h"
#include "chrome/test/ui/ui_test.h"
#include "net/base/net_util.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "v8/include/v8-testing.h"

namespace {

// See comments at the beginning of the file for the definition of switches.
const char kSiteSwitch[] = "site";
const char kStartPageSwitch[] = "startpage";
const char kEndPageSwitch[] = "endpage";
const char kListSwitch[] = "list";
const char kStartIndexSwitch[] = "startline";
const char kEndIndexSwitch[] = "endline";
const char kIterationSwitch[] = "iterations";
const char kContinuousLoadSwitch[] = "continuousload";
const char kMemoryUsageSwitch[] = "memoryusage";
const char kEndURLSwitch[] = "endurl";
const char kLogFileSwitch[] = "logfile";
const char kTimeoutSwitch[] = "timeout";
const char kNoPageDownSwitch[] = "nopagedown";
const char kNoClearProfileSwitch[] = "noclearprofile";
const char kSaveDebugLogSwitch[] = "savedebuglog";
const char kStressOptSwitch[] = "stress-opt";
const char kStressDeoptSwitch[] = "stress-deopt";

const char kDefaultServerUrl[] = "http://urllist.com";
std::string g_server_url;
const char kTestPage1[] = "page1.html";
const char kTestPage2[] = "page2.html";

// These are copied from v8 definitions as we cannot include them.
const char kV8LogFileSwitch[] = "logfile";
const char kV8LogFileDefaultName[] = "v8.log";

// String name of local chrome dll for looking up file information.
const wchar_t kChromeDll[] = L"chrome.dll";

bool g_append_page_id = false;
int32 g_start_page;
int32 g_end_page;
FilePath g_url_file_path;
int32 g_start_index = 1;
int32 g_end_index = kint32max;
int32 g_iterations = 1;
bool g_memory_usage = false;
bool g_continuous_load = false;
bool g_browser_existing = false;
bool g_page_down = true;
bool g_clear_profile = true;
std::string g_end_url;
FilePath g_log_file_path;
int g_timeout_ms = -1;
bool g_save_debug_log = false;
FilePath g_chrome_log_path;
FilePath g_v8_log_path;
FilePath g_test_log_path;
bool g_stand_alone = false;
bool g_stress_opt = false;
bool g_stress_deopt = false;

class PageLoadTest : public UITest {
 public:
  enum NavigationResult {
    NAVIGATION_ERROR = 0,
    NAVIGATION_SUCCESS,
    NAVIGATION_AUTH_NEEDED,
    NAVIGATION_TIME_OUT,
  };

  typedef struct {
    // These are results from the test automation that drives Chrome
    NavigationResult result;
    int crash_dump_count;
    // These are stability metrics recorded by Chrome itself
    bool browser_clean_exit;
    int browser_launch_count;
    int page_load_count;
    int browser_crash_count;
    int renderer_crash_count;
    int plugin_crash_count;
  } NavigationMetrics;

  PageLoadTest() {
    show_window_ = true;
  }

  void EnsureBrowserAndServer() {
    if (!g_browser_existing) {
      LaunchBrowserAndServer();
      g_browser_existing = true;
    }
  }

  // Load a URL in a browser tab and perform a couple of page down events.
  //   url_string:      The URL to navigate to. Accept URL as std::string here
  //                    because the url may also act as a test id and needs to
  //                    be logged in its original format even if invalid.
  //   log_file:        Log file for test results and possible crash dump
  //                    files. This file does not need to be opened in which
  //                    case nothing is logged.
  //   metrics_output:  Return metrics for the page load.
  //   keep_browser:    Set to true if the browser should be kept open after
  //                    loading the page.
  //   log_only_errors: Set to true if only errors should be logged otherwise
  //                    successful navigations will also be logged.
  bool NavigateToURLLogResult(const std::string& url_string,
                              std::ofstream& log_file,
                              NavigationMetrics* metrics_output,
                              bool keep_browser,
                              bool log_only_error) {
    GURL url(url_string);
    NavigationMetrics metrics = {NAVIGATION_ERROR};
    std::ofstream test_log;

    // Create a test log.
    g_test_log_path = FilePath(FILE_PATH_LITERAL("test_log.log"));
    test_log.open(g_test_log_path.value().c_str());

    // Get the version of Chrome we're running.
    std::string last_change;
#if defined(OS_WIN)
    // Check file version info for chrome dll.
    scoped_ptr<FileVersionInfo> file_info;
    file_info.reset(
        FileVersionInfo::CreateFileVersionInfo(FilePath(kChromeDll)));
    last_change = WideToASCII(file_info->last_change());
#elif defined(OS_LINUX) || defined(OS_MACOSX)
    // TODO(fmeawad): On Mac, the version retrieved here belongs to the test
    // module and not the chrome binary, need to be changed to chrome binary
    // instead.
    chrome::VersionInfo version_info;
    last_change = version_info.LastChange();
#endif  // !defined(OS_WIN)
    test_log << "Last Change: ";
    test_log << last_change << std::endl;

    // Log timestamp for test start.
    base::Time time_now = base::Time::Now();
    double time_start = time_now.ToDoubleT();
    test_log << "Test Start: ";
    test_log << base::TimeFormatFriendlyDateAndTime(time_now) << std::endl;

    // Make sure the browser is running.
    EnsureBrowserAndServer();

    // Log Browser Launched time.
    time_now = base::Time::Now();
    test_log << "browser_launched_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    int result = AUTOMATION_MSG_NAVIGATION_ERROR;
    // This is essentially what NavigateToURL does except we don't fire
    // assertion when page loading fails. We log the result instead.
    {
      // TabProxy should be released before Browser is closed.
      scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
      if (tab_proxy.get())
        result = tab_proxy->NavigateToURL(url);

      if (result == AUTOMATION_MSG_NAVIGATION_SUCCESS) {
        if (g_page_down) {
          // Page down twice.
          scoped_refptr<BrowserProxy> browser(
              automation()->GetBrowserWindow(0));
          if (browser.get()) {
            scoped_refptr<WindowProxy> window(browser->GetWindow());
            if (window.get()) {
              if (browser->BringToFront()) {
                // Sleep for 2 seconds between commands.
                // This used to be settable but the flag went away.
                int sleep_time_ms = 2000;
                window->SimulateOSKeyPress(ui::VKEY_NEXT, 0);
                base::PlatformThread::Sleep(sleep_time_ms);
                window->SimulateOSKeyPress(ui::VKEY_NEXT, 0);
                base::PlatformThread::Sleep(sleep_time_ms);
              }
            }
          }
        }
      }
    }

    // Log navigate complete time.
    time_now = base::Time::Now();
    test_log << "navigate_complete_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    if (!keep_browser) {
      CloseBrowserAndServer();
      g_browser_existing = false;
    }

    // Log end of test time.
    time_now = base::Time::Now();
    test_log << "total_duration_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    // Get navigation result and metrics, and optionally write to the log file
    // provided.  The log format is:
    // <url> <navigation_result> <browser_crash_count> <renderer_crash_count>
    // <plugin_crash_count> <crash_dump_count> [chrome_log=<path>
    // v8_log=<path>] crash_dump=<path>
    switch (result) {
      case AUTOMATION_MSG_NAVIGATION_ERROR:
        metrics.result = NAVIGATION_ERROR;
        break;
      case AUTOMATION_MSG_NAVIGATION_SUCCESS:
        metrics.result = NAVIGATION_SUCCESS;
        break;
      case AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED:
        metrics.result = NAVIGATION_AUTH_NEEDED;
        break;
      default:
        metrics.result = NAVIGATION_ERROR;
        break;
    }

    // Get crash dumps - don't delete them if logging.
    std::vector<FilePath> new_crash_dumps;
    CollectNewCrashDumps(new_crash_dumps, &metrics, !log_file.is_open());

    bool do_log = log_file.is_open() &&
                  (!log_only_error ||
                   metrics.result != NAVIGATION_SUCCESS ||
                   !new_crash_dumps.empty());
    if (do_log) {
      log_file << url_string;
      switch (metrics.result) {
        case NAVIGATION_ERROR:
          log_file << " error";
          break;
        case NAVIGATION_SUCCESS:
          log_file << " success";
          break;
        case NAVIGATION_AUTH_NEEDED:
          log_file << " auth_needed";
          break;
        case NAVIGATION_TIME_OUT:
          log_file << " timeout";
          break;
        default:
          break;
      }
    }

#if !defined(OS_MACOSX)  // Not used by mac chromebot.
    // Get stability metrics recorded by Chrome itself.
    GetStabilityMetrics(&metrics);
#endif

    if (do_log) {
      log_file << " " << metrics.browser_crash_count \
               // The renderer crash count is flaky due to 1183283.
               // Ignore the count since we also catch crash by
               // crash_dump_count.
               << " " << 0 \
               << " " << metrics.plugin_crash_count \
               << " " << metrics.crash_dump_count;
    }

    // Close test log.
    test_log.close();

    if (do_log && g_save_debug_log && !g_continuous_load)
      SaveDebugLogs(log_file);

    // Log revision information for Chrome build under test.
    if (do_log)
      log_file << " " << "revision=" << last_change;

    if (do_log) {
      for (size_t i = 0; i < new_crash_dumps.size(); i++)
        log_file << " crash_dump=" << new_crash_dumps[i].value().c_str();
    }

    if (do_log)
      log_file << std::endl;

    if (metrics_output)
      *metrics_output = metrics;

    return do_log;
  }

  void NavigateThroughPageID(std::ofstream& log_file) {
    if (g_append_page_id) {
      // For usage 2
      for (int i = g_start_page; i <= g_end_page; ++i) {
        const char* server = g_server_url.empty() ? kDefaultServerUrl :
            g_server_url.c_str();
        std::string test_page_url(
            StringPrintf("%s/page?id=%d", server, i));
        NavigateToURLLogResult(
            test_page_url, log_file, NULL, g_continuous_load, false);
      }
    } else {
      // Don't run if single process mode.
      // Also don't run if running as a standalone program which is for
      // distributed testing, to avoid mistakenly hitting web sites with many
      // instances.
      if (ProxyLauncher::in_process_renderer() || g_stand_alone)
        return;
      // For usage 1
      NavigationMetrics metrics;
      if (g_timeout_ms == -1)
        g_timeout_ms = 2000;

      // Though it would be nice to test the page down code path in usage 1,
      // enabling page down adds several seconds to the test and does not seem
      // worth the tradeoff. It is also potentially disruptive when running the
      // test in the background as it will send the event to the window that
      // has focus.
      g_page_down = false;

      FilePath sample_data_dir = GetSampleDataDir();
      FilePath test_page_1 = sample_data_dir.AppendASCII(kTestPage1);
      FilePath test_page_2 = sample_data_dir.AppendASCII(kTestPage2);

      GURL test_url_1 = net::FilePathToFileURL(test_page_1);
      GURL test_url_2 = net::FilePathToFileURL(test_page_2);

      // Convert back to string so that all calls to navigate are the same.
      const std::string test_url_1_string = test_url_1.spec();
      const std::string test_url_2_string = test_url_2.spec();

      NavigateToURLLogResult(
          test_url_1_string, log_file, &metrics, g_continuous_load, false);
      // Verify everything is fine
      EXPECT_EQ(NAVIGATION_SUCCESS, metrics.result);
      EXPECT_EQ(0, metrics.crash_dump_count);
      EXPECT_TRUE(metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      // Both starting page and test_url_1 are loaded.
      EXPECT_EQ(2, metrics.page_load_count);
      EXPECT_EQ(0, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      // Go to "about:crash"
      NavigateToURLLogResult(chrome::kAboutCrashURL,
                             log_file,
                             &metrics,
                             g_continuous_load,
                             false);
      // Found a crash dump
      EXPECT_EQ(1, metrics.crash_dump_count) << kFailedNoCrashService;
      // Browser did not crash, and exited cleanly.
      EXPECT_TRUE(metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      // Only the renderer should have crashed.
      EXPECT_EQ(0, metrics.browser_crash_count);
      EXPECT_EQ(1, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      NavigateToURLLogResult(
          test_url_2_string, log_file, &metrics, g_continuous_load, false);
      // The data on previous crash should be cleared and we should get
      // metrics for a successful page load.
      EXPECT_EQ(NAVIGATION_SUCCESS, metrics.result);
      EXPECT_EQ(0, metrics.crash_dump_count);
      EXPECT_TRUE(metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_launch_count);
      EXPECT_EQ(0, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);

      // Verify metrics service does what we need when browser process crashes.
      LaunchBrowserAndServer();
      {
        // TabProxy should be released before Browser is closed.
        scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
        EXPECT_TRUE(tab_proxy.get());
        if (tab_proxy.get()) {
          EXPECT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS,
                    tab_proxy->NavigateToURL(GURL(test_url_1)));
        }
      }
      // Kill browser process.
      base::ProcessHandle browser_process = process();
      base::KillProcess(browser_process, 0, false);

      GetStabilityMetrics(&metrics);
      // This is not a clean shutdown.
      EXPECT_FALSE(metrics.browser_clean_exit);
      EXPECT_EQ(1, metrics.browser_crash_count);
      EXPECT_EQ(0, metrics.renderer_crash_count);
      EXPECT_EQ(0, metrics.plugin_crash_count);
      // Relaunch browser so UITest does not fire assertion during TearDown.
      LaunchBrowserAndServer();
    }
  }

  // For usage 3
  void NavigateThroughURLList(std::ofstream& log_file) {
    std::ifstream file(g_url_file_path.value().c_str());
    ASSERT_TRUE(file.is_open());

    for (int line_index = 1;
         line_index <= g_end_index && !file.eof();
         ++line_index) {
      std::string url_str;
      std::getline(file, url_str);

      if (file.fail())
        break;

      if (g_start_index <= line_index) {
        if (g_stress_opt || g_stress_deopt) {
          // Make sure the browser is running to communicate the stress
          // setting.
          EnsureBrowserAndServer();
          v8::Testing::StressType stress_type =
              g_stress_opt
                  ? v8::Testing::kStressTypeOpt
                  : v8::Testing::kStressTypeDeopt;
          scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
          if (tab_proxy.get()) {
            tab_proxy->JavaScriptStressTestControl(
                kJavaScriptStressTestSetStressRunType, stress_type);
          }

          bool success = true;
          // Load each page a number of times and keep the same browser open
          // for these loads. This loop will end if an error is encountered
          // during one of the loads and the error logged.
          for (int i = 0;
               i < v8::Testing::GetStressRuns() && success;
               i++) {
            bool last_load = (i == (v8::Testing::GetStressRuns() - 1));
            bool keep_browser = !last_load || g_continuous_load;
            bool log_only_error = !last_load;
            NavigationMetrics metrics;

            scoped_refptr<TabProxy> tab_proxy(GetActiveTab());
            if (tab_proxy.get()) {
              tab_proxy->JavaScriptStressTestControl(
                  kJavaScriptStressTestPrepareStressRun, i);
            }
            bool did_log_error;
            did_log_error = NavigateToURLLogResult(url_str,
                                                   log_file,
                                                   &metrics,
                                                   keep_browser,
                                                   log_only_error);
            success = metrics.result == NAVIGATION_SUCCESS && !did_log_error;
          }
        } else {
          NavigateToURLLogResult(
              url_str, log_file, NULL, g_continuous_load, false);
        }
      }
    }

    file.close();
  }

 protected:
  // Call the base class's SetUp method and initialize our own class members.
  virtual void SetUp() {
    // Set UI Test members before setting up browser.
    clear_profile_ = g_clear_profile;

    UITest::SetUp();
    g_browser_existing = true;

    // Initialize crash_dumps_dir_path_.
    PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dumps_dir_path_);
    file_util::FileEnumerator enumerator(crash_dumps_dir_path_,
                                         false,  // not recursive
                                         file_util::FileEnumerator::FILES);
    for (FilePath path = enumerator.Next(); !path.value().empty();
         path = enumerator.Next()) {
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmp")))
        crash_dumps_[path.BaseName()] = true;
    }
  }

  FilePath ConstructSavedDebugLogPath(const FilePath& debug_log_path,
                                      int index) {
    std::string suffix("_");
    suffix.append(base::IntToString(index));
    return debug_log_path.InsertBeforeExtensionASCII(suffix);
  }

  void SaveDebugLog(const FilePath& log_path, const std::wstring& log_id,
                    std::ofstream& log_file, int index) {
    if (!log_path.empty()) {
      FilePath saved_log_file_path =
          ConstructSavedDebugLogPath(log_path, index);
      if (file_util::Move(log_path, saved_log_file_path)) {
        log_file << " " << log_id << "=" << saved_log_file_path.value();
      }
    }
  }

  // Rename the chrome and v8 debug log files if existing, and save the file
  // paths in the log_file provided.
  void SaveDebugLogs(std::ofstream& log_file) {
    static int url_count = 1;
    SaveDebugLog(g_chrome_log_path, L"chrome_log", log_file, url_count);
    SaveDebugLog(g_v8_log_path, L"v8_log", log_file, url_count);
    SaveDebugLog(g_test_log_path, L"test_log", log_file, url_count);
    url_count++;
  }

  // Delete a crash dump file.
  void DeleteCrashDump(FilePath crash_dump_file_name) {
    FilePath crash_dump_file_path(crash_dumps_dir_path_);
    crash_dump_file_path = crash_dump_file_path.Append(crash_dump_file_name);
    FilePath crash_text_file_path =
        crash_dump_file_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));

    ASSERT_TRUE(file_util::DieFileDie(crash_dump_file_path, false));
    ASSERT_TRUE(file_util::DieFileDie(crash_text_file_path, false));
  }

  bool HasNewCrashDumps() {
    file_util::FileEnumerator enumerator(crash_dumps_dir_path_,
                                         false,  // not recursive
                                         file_util::FileEnumerator::FILES);
    for (FilePath path = enumerator.Next(); !path.value().empty();
         path = enumerator.Next()) {
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmp")) &&
          !crash_dumps_[path.BaseName()]) {
        return true;
      }
    }

    return false;
  }

  // Check whether there are new .dmp files. Return the list and optionally
  // delete them afterwards.
  void CollectNewCrashDumps(std::vector<FilePath>& new_crash_dumps,
                            NavigationMetrics* metrics,
                            bool delete_dumps) {
    int num_dumps = 0;

    file_util::FileEnumerator enumerator(crash_dumps_dir_path_,
                                         false,  // not recursive
                                         file_util::FileEnumerator::FILES);
    for (FilePath path = enumerator.Next(); !path.value().empty();
         path = enumerator.Next()) {
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmp")) &&
          !crash_dumps_[path.BaseName()]) {
        crash_dumps_[path.BaseName()] = true;
        FilePath crash_dump_file_path(crash_dumps_dir_path_);
        crash_dump_file_path = crash_dump_file_path.Append(path.BaseName());
        new_crash_dumps.push_back(crash_dump_file_path);
        if (delete_dumps)
          DeleteCrashDump(path.BaseName());
        num_dumps++;
      }
    }
    if (metrics)
      metrics->crash_dump_count = num_dumps;
  }

  // Get a PrefService whose contents correspond to the Local State file
  // that was saved by the app as it closed.  The caller takes ownership of the
  // returned PrefService object.
  PrefService* GetLocalState() {
    FilePath path = user_data_dir().Append(chrome::kLocalStateFilename);
    return PrefServiceMockBuilder().WithUserFilePrefs(path).Create();
  }

  void GetStabilityMetrics(NavigationMetrics* metrics) {
    if (!metrics)
      return;
    scoped_ptr<PrefService> local_state(GetLocalState());
    if (!local_state.get())
      return;
    local_state->RegisterBooleanPref(prefs::kStabilityExitedCleanly, false);
    local_state->RegisterIntegerPref(prefs::kStabilityLaunchCount, -1);
    local_state->RegisterIntegerPref(prefs::kStabilityPageLoadCount, -1);
    local_state->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
    local_state->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);

    metrics->browser_clean_exit =
        local_state->GetBoolean(prefs::kStabilityExitedCleanly);
    metrics->browser_launch_count =
        local_state->GetInteger(prefs::kStabilityLaunchCount);
    metrics->page_load_count =
        local_state->GetInteger(prefs::kStabilityPageLoadCount);
    metrics->browser_crash_count =
        local_state->GetInteger(prefs::kStabilityCrashCount);
    metrics->renderer_crash_count =
        local_state->GetInteger(prefs::kStabilityRendererCrashCount);
    // TODO(huanr)
    metrics->plugin_crash_count = 0;

    if (!metrics->browser_clean_exit)
      metrics->browser_crash_count++;
  }

  FilePath GetSampleDataDir() {
    FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    test_dir = test_dir.AppendASCII("reliability");
    test_dir = test_dir.AppendASCII("sample_pages");
    return test_dir;
  }

  // The pathname of Chrome's crash dumps directory.
  FilePath crash_dumps_dir_path_;

  // The set of all the crash dumps we have seen.  Each crash generates a
  // .dmp and a .txt file in the crash dumps directory.  We only store the
  // .dmp files in this set.
  //
  // The set is implemented as a std::map.  The key is the file name, and
  // the value is false (the file is not in the set) or true (the file is
  // in the set).  The initial value for any key in std::map is 0 (false),
  // which in this case means a new file is not in the set initially,
  // exactly the semantics we want.
  std::map<FilePath, bool> crash_dumps_;
};

TEST_F(PageLoadTest, Reliability) {
  std::ofstream log_file;

  if (!g_log_file_path.empty()) {
    log_file.open(g_log_file_path.value().c_str());
  }

  for (int k = 0; k < g_iterations; ++k) {
    if (g_url_file_path.empty()) {
      NavigateThroughPageID(log_file);
    } else {
      NavigateThroughURLList(log_file);
    }
  }

  if (!g_end_url.empty()) {
    NavigateToURLLogResult(
        g_end_url, log_file, NULL, g_continuous_load, false);
  }

  log_file.close();
}

}  // namespace

namespace {
  void ReportHandler(const std::string& str) {
    // Ignore report events.
  }
}

void SetPageRange(const CommandLine& parsed_command_line) {
  // If calling into this function, we are running as a standalone program.
  g_stand_alone = true;

  // Since we use --enable-dcheck for reliability tests, suppress the error
  // dialog in the test process.
  logging::SetLogReportHandler(ReportHandler);

  if (parsed_command_line.HasSwitch(kStartPageSwitch)) {
    ASSERT_TRUE(parsed_command_line.HasSwitch(kEndPageSwitch));
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kStartPageSwitch),
                          &g_start_page));
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kEndPageSwitch),
                          &g_end_page));
    ASSERT_TRUE(g_start_page > 0 && g_end_page > 0);
    ASSERT_TRUE(g_start_page < g_end_page);
    g_append_page_id = true;
  } else {
    ASSERT_FALSE(parsed_command_line.HasSwitch(kEndPageSwitch));
  }

  if (parsed_command_line.HasSwitch(kSiteSwitch)) {
    g_server_url = parsed_command_line.GetSwitchValueASCII(kSiteSwitch);
  }

  if (parsed_command_line.HasSwitch(kStartIndexSwitch)) {
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kStartIndexSwitch),
                          &g_start_index));
    ASSERT_GT(g_start_index, 0);
  }

  if (parsed_command_line.HasSwitch(kEndIndexSwitch)) {
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kEndIndexSwitch),
                          &g_end_index));
    ASSERT_GT(g_end_index, 0);
  }

  ASSERT_TRUE(g_end_index >= g_start_index);

  if (parsed_command_line.HasSwitch(kListSwitch))
    g_url_file_path = parsed_command_line.GetSwitchValuePath(kListSwitch);

  if (parsed_command_line.HasSwitch(kIterationSwitch)) {
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kIterationSwitch),
                          &g_iterations));
    ASSERT_GT(g_iterations, 0);
  }

  if (parsed_command_line.HasSwitch(kMemoryUsageSwitch))
    g_memory_usage = true;

  if (parsed_command_line.HasSwitch(kContinuousLoadSwitch))
    g_continuous_load = true;

  if (parsed_command_line.HasSwitch(kEndURLSwitch))
    g_end_url = parsed_command_line.GetSwitchValueASCII(kEndURLSwitch);

  if (parsed_command_line.HasSwitch(kLogFileSwitch))
    g_log_file_path = parsed_command_line.GetSwitchValuePath(kLogFileSwitch);

  if (parsed_command_line.HasSwitch(kTimeoutSwitch)) {
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                          kTimeoutSwitch),
                          &g_timeout_ms));
    ASSERT_GT(g_timeout_ms, 0);
  }

  if (parsed_command_line.HasSwitch(kNoPageDownSwitch))
    g_page_down = false;

  if (parsed_command_line.HasSwitch(kNoClearProfileSwitch))
    g_clear_profile = false;

  if (parsed_command_line.HasSwitch(kSaveDebugLogSwitch)) {
    g_save_debug_log = true;
    g_chrome_log_path = logging::GetLogFileName();
    // We won't get v8 log unless --no-sandbox is specified.
    if (parsed_command_line.HasSwitch(switches::kNoSandbox)) {
      PathService::Get(base::DIR_CURRENT, &g_v8_log_path);
      g_v8_log_path = g_v8_log_path.AppendASCII(kV8LogFileDefaultName);
      // The command line switch may override the default v8 log path.
      if (parsed_command_line.HasSwitch(switches::kJavaScriptFlags)) {
        CommandLine v8_command_line(
            parsed_command_line.GetSwitchValuePath(switches::kJavaScriptFlags));
        if (v8_command_line.HasSwitch(kV8LogFileSwitch)) {
          g_v8_log_path = v8_command_line.GetSwitchValuePath(kV8LogFileSwitch);
          if (!file_util::AbsolutePath(&g_v8_log_path))
            g_v8_log_path = FilePath();
        }
      }
    }
  }

  if (parsed_command_line.HasSwitch(kStressOptSwitch)) {
    g_stress_opt = true;
  }
  if (parsed_command_line.HasSwitch(kStressDeoptSwitch)) {
    g_stress_deopt = true;
  }
}
