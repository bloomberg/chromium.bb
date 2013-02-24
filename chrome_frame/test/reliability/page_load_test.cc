// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides reliablity tests which run for ChromeFrame.
//
// Usage:
// <reliability test exe> --list=file --startline=start --endline=end [...]
// Upon invocation, it visits each of the URLs on line numbers between start
// and end, inclusive, stored in the input file. The line number starts from 1.
//
// Optional Switches:
// --iterations=num: goes through the list of URLs constructed in usage 2 or 3
//                   num times.
// --memoryusage: prints out memory usage when visiting each page.
// --logfile=filepath: saves the visit log to the specified path.
// --timeout=seconds: time out as specified in seconds during each
//                    page load.
// --nopagedown: won't simulate page down key presses after page load.
// --noclearprofile: do not clear profile dir before firing up each time.
// --savedebuglog: save Chrome, V8, and test debug log for each page loaded.
#include <fstream>
#include <ostream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/path_service.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/pref_value_store.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_file_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/ie_event_sink.h"
#include "chrome_frame/test/reliability/page_load_test.h"
#include "chrome_frame/utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrCaseEq;
using testing::StrCaseNe;

namespace {

// See comments at the beginning of the file for the definition of switches.
const char kListSwitch[] = "list";
const char kStartIndexSwitch[] = "startline";
const char kEndIndexSwitch[] = "endline";
const char kIterationSwitch[] = "iterations";
const char kContinuousLoadSwitch[] = "continuousload";
const char kMemoryUsageSwitch[] = "memoryusage";
const char kLogFileSwitch[] = "logfile";
const char kTimeoutSwitch[] = "timeout";
const char kNoPageDownSwitch[] = "nopagedown";
const char kNoClearProfileSwitch[] = "noclearprofile";
const char kSaveDebugLogSwitch[] = "savedebuglog";

// These are copied from v8 definitions as we cannot include them.
const char kV8LogFileSwitch[] = "logfile";
const char kV8LogFileDefaultName[] = "v8.log";

// String name of local chrome dll for looking up file information.
const wchar_t kChromeDll[] = L"chrome.dll";

base::FilePath g_url_file_path;
int32 g_start_index = 1;
int32 g_end_index = kint32max;
int32 g_iterations = 1;
bool g_memory_usage = false;
bool g_page_down = true;
bool g_clear_profile = true;
std::string g_end_url;
base::FilePath g_log_file_path;
bool g_save_debug_log = false;
base::FilePath g_chrome_log_path;
base::FilePath g_v8_log_path;
base::FilePath g_test_log_path;
bool g_stand_alone = false;

const int kUrlNavigationTimeoutSeconds = 20;
int g_timeout_seconds = kUrlNavigationTimeoutSeconds;

// Mocks document complete and load events.
class MockLoadListener : public chrome_frame_test::IEEventListener {
 public:
  MOCK_METHOD1(OnDocumentComplete, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD1(OnLoad, void (const wchar_t* url));  // NOLINT
  MOCK_METHOD0(OnQuit, void ());  // NOLINT

 private:
  virtual void OnDocumentComplete(IDispatch* dispatch, VARIANT* url) {
    if (url->bstrVal)
      OnDocumentComplete(url->bstrVal);
  }
};

ACTION_P(QuitIE, event_sink) {
  EXPECT_HRESULT_SUCCEEDED(event_sink->CloseWebBrowser());
}

class PageLoadTest : public testing::Test {
 public:
  enum NavigationResult {
    NAVIGATION_ERROR = 0,
    NAVIGATION_SUCCESS,
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

  PageLoadTest() {}

  // Accept URL as std::string here because the url may also act as a test id
  // and needs to be logged in its original format even if invalid.
  void NavigateToURLLogResult(const std::string& url_string,
                              std::ofstream& log_file,
                              NavigationMetrics* metrics_output) {
    GURL url(url_string);
    NavigationMetrics metrics = {NAVIGATION_ERROR};
    std::ofstream test_log;

    // Create a test log.
    g_test_log_path = base::FilePath(FILE_PATH_LITERAL("test_log.log"));
    test_log.open(g_test_log_path.value().c_str());

    // Check file version info for chrome dll.
    scoped_ptr<FileVersionInfo> file_info;
#if defined(OS_WIN)
    file_info.reset(
        FileVersionInfo::CreateFileVersionInfo(base::FilePath(kChromeDll)));
#elif defined(OS_LINUX) || defined(OS_MACOSX)
    // TODO(fmeawad): the version retrieved here belongs to the test module and
    // not the chrome binary, need to be changed to chrome binary instead.
    file_info.reset(FileVersionInfo::CreateFileVersionInfoForCurrentModule());
#endif  // !defined(OS_WIN)
    std::wstring last_change = file_info->last_change();
    test_log << "Last Change: ";
    test_log << last_change << std::endl;


    // Log timestamp for test start.
    base::Time time_now = base::Time::Now();
    double time_start = time_now.ToDoubleT();
    test_log << "Test Start: ";
    test_log << base::TimeFormatFriendlyDateAndTime(time_now) << std::endl;

    HRESULT hr = E_FAIL;

    chrome_frame_test::TimedMsgLoop message_loop;

    // Launch IE.
    base::win::ScopedComPtr<IWebBrowser2> web_browser2;
    hr = chrome_frame_test::LaunchIEAsComServer(web_browser2.Receive());
    EXPECT_HRESULT_SUCCEEDED(hr);
    EXPECT_TRUE(web_browser2.get() != NULL);
    web_browser2->put_Visible(VARIANT_TRUE);

    // Log Browser Launched time.
    time_now = base::Time::Now();
    test_log << "browser_launched_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    bool is_chrome_frame_navigation =
        StartsWith(UTF8ToWide(url.spec()), kChromeProtocolPrefix, true);

    CComObjectStack<chrome_frame_test::IEEventSink> ie_event_sink;
    MockLoadListener load_listener;
    // Disregard any interstitial about:blank loads.
    EXPECT_CALL(load_listener, OnDocumentComplete(StrCaseEq(L"about:blank")))
        .Times(testing::AnyNumber());

    // Note that we can't compare the loaded url directly with the given url
    // because the page may have redirected us to a different page, e.g.
    // www.google.com -> www.google.ca.
    if (is_chrome_frame_navigation) {
      EXPECT_CALL(load_listener, OnDocumentComplete(testing::_));
      EXPECT_CALL(load_listener, OnLoad(testing::_))
          .WillOnce(QuitIE(&ie_event_sink));
    } else {
      EXPECT_CALL(load_listener, OnDocumentComplete(StrCaseNe(L"about:blank")))
          .WillOnce(QuitIE(&ie_event_sink));
    }
    EXPECT_CALL(load_listener, OnQuit()).WillOnce(QUIT_LOOP(message_loop));

    // Attach the sink and navigate.
    ie_event_sink.set_listener(&load_listener);
    ie_event_sink.Attach(web_browser2);
    hr = ie_event_sink.Navigate(UTF8ToWide(url.spec()));
    if (SUCCEEDED(hr)) {
      message_loop.RunFor(base::TimeDelta::FromSeconds(g_timeout_seconds));
      if (!message_loop.WasTimedOut())
        metrics.result = NAVIGATION_SUCCESS;
    }

    // Log navigate complete time.
    time_now = base::Time::Now();
    test_log << "navigate_complete_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    // Close IE.
    ie_event_sink.set_listener(NULL);
    ie_event_sink.Uninitialize();
    chrome_frame_test::CloseAllIEWindows();

    // Log end of test time.
    time_now = base::Time::Now();
    test_log << "total_duration_seconds=";
    test_log << (time_now.ToDoubleT() - time_start) << std::endl;

    // Get navigation result and metrics, and optionally write to the log file
    // provided.  The log format is:
    // <url> <navigation_result> <browser_crash_count> <renderer_crash_count>
    // <plugin_crash_count> <crash_dump_count> [chrome_log=<path>
    // v8_log=<path>] crash_dump=<path>
    if (log_file.is_open()) {
      log_file << url_string;
      switch (metrics.result) {
        case NAVIGATION_ERROR:
          log_file << " error";
          break;
        case NAVIGATION_SUCCESS:
          log_file << " success";
          break;
        default:
          break;
      }
    }

    // Get stability metrics recorded by Chrome itself.
    if (is_chrome_frame_navigation) {
      GetStabilityMetrics(&metrics);
    }

    if (log_file.is_open()) {
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

    if (log_file.is_open() && g_save_debug_log)
      SaveDebugLogs(log_file);

    // Log revision information for Chrome build under test.
    log_file << " " << "revision=" << last_change;

    // Get crash dumps.
    LogOrDeleteNewCrashDumps(log_file, &metrics);

    if (log_file.is_open()) {
      log_file << std::endl;
    }

    if (metrics_output) {
      *metrics_output = metrics;
    }
  }

  void NavigateThroughURLList(std::ofstream& log_file) {
    std::ifstream file(g_url_file_path.value().c_str());
    ASSERT_TRUE(file.is_open());

    // We navigate to URLs in the following order.
    // CF -> CF -> host -> CF -> CF -> host.
    for (int line_index = 1;
         line_index <= g_end_index && !file.eof();
         ++line_index) {
      std::string url_str;
      std::getline(file, url_str);

      if (file.fail()) {
        break;
      }

      // Every 3rd URL goes into the host browser.
      if (line_index % 3 != 0) {
        std::string actual_url;
        actual_url = WideToUTF8(kChromeProtocolPrefix);
        actual_url += url_str;
        url_str = actual_url;
      }

      if (g_start_index <= line_index) {
        NavigateToURLLogResult(url_str, log_file, NULL);
      }
    }

    file.close();
  }

 protected:
  virtual void SetUp() {
    // Initialize crash_dumps_dir_path_.
    PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dumps_dir_path_);
    file_util::FileEnumerator enumerator(crash_dumps_dir_path_,
                                         false,  // not recursive
                                         file_util::FileEnumerator::FILES);
    for (base::FilePath path = enumerator.Next(); !path.value().empty();
         path = enumerator.Next()) {
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmp")))
        crash_dumps_[path.BaseName()] = true;
    }

    if (g_clear_profile) {
      base::FilePath user_data_dir;
      chrome::GetChromeFrameUserDataDirectory(&user_data_dir);
      ASSERT_TRUE(file_util::DieFileDie(user_data_dir, true));
    }

    SetConfigBool(kChromeFrameHeadlessMode, true);
    SetConfigBool(kAllowUnsafeURLs, true);
  }

  virtual void TearDown() {
    DeleteConfigValue(kChromeFrameHeadlessMode);
    DeleteConfigValue(kAllowUnsafeURLs);
  }

  base::FilePath ConstructSavedDebugLogPath(const base::FilePath& debug_log_path,
                                            int index) {
    std::string suffix("_");
    suffix.append(base::IntToString(index));
    return debug_log_path.InsertBeforeExtensionASCII(suffix);
  }

  void SaveDebugLog(const base::FilePath& log_path, const std::wstring& log_id,
                    std::ofstream& log_file, int index) {
    if (!log_path.empty()) {
      base::FilePath saved_log_file_path =
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

  // If a log_file is provided, log the crash dump with the given path;
  // otherwise, delete the crash dump file.
  void LogOrDeleteCrashDump(std::ofstream& log_file,
                            base::FilePath crash_dump_file_name) {
    base::FilePath crash_dump_file_path(crash_dumps_dir_path_);
    crash_dump_file_path = crash_dump_file_path.Append(crash_dump_file_name);
    base::FilePath crash_text_file_path =
        crash_dump_file_path.ReplaceExtension(FILE_PATH_LITERAL("txt"));

    if (log_file.is_open()) {
      crash_dumps_[crash_dump_file_name] = true;
      log_file << " crash_dump=" << crash_dump_file_path.value().c_str();
    } else {
      ASSERT_TRUE(file_util::DieFileDie(
          crash_dump_file_path, false));
      ASSERT_TRUE(file_util::DieFileDie(
          crash_text_file_path, false));
    }
  }

  // Check whether there are new .dmp files. Additionally, write
  //     " crash_dump=<full path name of the .dmp file>"
  // to log_file.
  void LogOrDeleteNewCrashDumps(std::ofstream& log_file,
                                NavigationMetrics* metrics) {
    int num_dumps = 0;

    file_util::FileEnumerator enumerator(crash_dumps_dir_path_,
                                         false,  // not recursive
                                         file_util::FileEnumerator::FILES);
    for (base::FilePath path = enumerator.Next(); !path.value().empty();
         path = enumerator.Next()) {
      if (path.MatchesExtension(FILE_PATH_LITERAL(".dmp")) &&
          !crash_dumps_[path.BaseName()]) {
        LogOrDeleteCrashDump(log_file, path.BaseName());
        num_dumps++;
      }
    }
    if (metrics)
      metrics->crash_dump_count = num_dumps;
  }

  // Get a PrefService whose contents correspond to the Local State file
  // that was saved by the app as it closed.  The caller takes ownership of the
  // returned PrefService object.
  PrefService* GetLocalState(PrefRegistry* registry) {
    base::FilePath path;
    chrome::GetChromeFrameUserDataDirectory(&path);
    PrefServiceMockBuilder builder;
    builder.WithUserFilePrefs(
        path,
        JsonPrefStore::GetTaskRunnerForFile(
            path, content::BrowserThread::GetBlockingPool()));
    return builder.Create(registry);
  }

  void GetStabilityMetrics(NavigationMetrics* metrics) {
    if (!metrics)
      return;
    scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple();
    registry->RegisterBooleanPref(prefs::kStabilityExitedCleanly, false);
    registry->RegisterIntegerPref(prefs::kStabilityLaunchCount, -1);
    registry->RegisterIntegerPref(prefs::kStabilityPageLoadCount, -1);
    registry->RegisterIntegerPref(prefs::kStabilityCrashCount, 0);
    registry->RegisterIntegerPref(prefs::kStabilityRendererCrashCount, 0);

    scoped_ptr<PrefService> local_state(GetLocalState(registry));
    if (!local_state.get())
      return;

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

  base::FilePath GetSampleDataDir() {
    base::FilePath test_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_dir);
    test_dir = test_dir.AppendASCII("reliability");
    test_dir = test_dir.AppendASCII("sample_pages");
    return test_dir;
  }

  // The pathname of Chrome's crash dumps directory.
  base::FilePath crash_dumps_dir_path_;

  // The set of all the crash dumps we have seen.  Each crash generates a
  // .dmp and a .txt file in the crash dumps directory.  We only store the
  // .dmp files in this set.
  //
  // The set is implemented as a std::map.  The key is the file name, and
  // the value is false (the file is not in the set) or true (the file is
  // in the set).  The initial value for any key in std::map is 0 (false),
  // which in this case means a new file is not in the set initially,
  // exactly the semantics we want.
  std::map<base::FilePath, bool> crash_dumps_;
};

TEST_F(PageLoadTest, IEFullTabMode_Reliability) {
  std::ofstream log_file;

  if (!g_log_file_path.empty()) {
    log_file.open(g_log_file_path.value().c_str());
  }

  EXPECT_FALSE(g_url_file_path.empty());

  for (int k = 0; k < g_iterations; ++k) {
    NavigateThroughURLList(log_file);
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

  if (parsed_command_line.HasSwitch(kLogFileSwitch))
    g_log_file_path = parsed_command_line.GetSwitchValuePath(kLogFileSwitch);

  if (parsed_command_line.HasSwitch(kTimeoutSwitch)) {
    ASSERT_TRUE(
        base::StringToInt(parsed_command_line.GetSwitchValueASCII(
                              kTimeoutSwitch),
                          &g_timeout_seconds));
    ASSERT_GT(g_timeout_seconds, 0);
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
            g_v8_log_path = base::FilePath();
        }
      }
    }
  }
}
