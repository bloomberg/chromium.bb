// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_piece.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/perf/perf_test.h"
#include "chrome/test/ui/ui_perf_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#ifndef NDEBUG
static const int kTestIterations = 2;
static const int kDatabaseTestIterations = 2;
static const int kWebPageReplayIterations = 2;
#else
static const int kTestIterations = 10;
// For some unknown reason, the DB perf tests are much much slower on the
// Vista perf bot, so we have to cut down the number of iterations to 5
// to make sure each test finishes in less than 10 minutes.
static const int kDatabaseTestIterations = 5;
static const int kWebPageReplayIterations = 5;
#endif
static const int kIDBTestIterations = 5;

// URL at which data files may be found for HTTP tests.  The document root of
// this URL's server should point to data/page_cycler/.
static const char kBaseUrl[] = "http://localhost:8000/";

// The following port numbers must match those in
//     src/tools/python/google/webpagereplay_utils.py.
static const char kWebPageReplayHttpPort[] = "8080";
static const char kWebPageReplayHttpsPort[] = "8413";

namespace {

void PopulateBufferCache(const base::FilePath& test_dir) {
  // This will recursively walk the directory given and read all the
  // files it finds.  This is done so the system file cache is likely
  // to have as much loaded as possible.  Without this, the tests of
  // this build gets one set of timings and then the reference build
  // test, gets slightly faster ones (even if the reference build is
  // the same binary).  The hope is by forcing all the possible data
  // into the cache we equalize the tests for comparing timing data.

  // We don't want to walk into .svn dirs, so we have to do the tree walk
  // ourselves.

  std::vector<base::FilePath> dirs;
  dirs.push_back(test_dir);
  const base::FilePath svn_dir(FILE_PATH_LITERAL(".svn"));

  for (size_t idx = 0; idx < dirs.size(); ++idx) {
    file_util::FileEnumerator dir_enumerator(dirs[idx], false,
                                      file_util::FileEnumerator::DIRECTORIES);
    base::FilePath path;
    for (path = dir_enumerator.Next();
         !path.empty();
         path = dir_enumerator.Next()) {
      if (path.BaseName() != svn_dir)
        dirs.push_back(path);
    }
  }

  unsigned int loaded = 0;

  // We seem to have some files in the data dirs that are just there for
  // reference, make a quick attempt to skip them by matching suffixes.
  std::vector<base::FilePath::StringType> ignore_suffixes;
  ignore_suffixes.push_back(FILE_PATH_LITERAL(".orig.html"));
  ignore_suffixes.push_back(FILE_PATH_LITERAL(".html-original"));

  std::vector<base::FilePath>::const_iterator iter;
  for (iter = dirs.begin(); iter != dirs.end(); ++iter) {
    file_util::FileEnumerator file_enumerator(*iter, false,
                                              file_util::FileEnumerator::FILES);
    base::FilePath path;
    for (path = file_enumerator.Next();
         !path.empty();
         path = file_enumerator.Next()) {
      const base::FilePath base_name = path.BaseName();
      const size_t base_name_size = base_name.value().size();

      bool should_skip = false;
      std::vector<base::FilePath::StringType>::const_iterator ignore_iter;
      for (ignore_iter = ignore_suffixes.begin();
           ignore_iter != ignore_suffixes.end();
           ++ignore_iter) {
        const base::FilePath::StringType &suffix = *ignore_iter;

        if ((base_name_size > suffix.size()) &&
            (base_name.value().compare(base_name_size - suffix.size(),
                                       suffix.size(), suffix) == 0)) {
          should_skip = true;
          break;
        }
      }
      if (should_skip)
        continue;

      // Read the file fully to get it into the cache.
      // We don't care what the contents are.
      if (file_util::ReadFileToString(path, NULL))
        ++loaded;
    }
  }
  VLOG(1) << "Buffer cache should be primed with " << loaded << " files.";
}

class PageCyclerTest : public UIPerfTest {
 public:
  PageCyclerTest() : print_times_only_(false),
                     num_test_iterations_(kTestIterations) {
    show_window_ = true;
    dom_automation_enabled_ = true;

    const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
    if (parsed_command_line.HasSwitch(switches::kPageCyclerIterations)) {
      std::string str = parsed_command_line.GetSwitchValueASCII(
          switches::kPageCyclerIterations);
      base::StringToInt(str, &num_test_iterations_);
    }
  }

  virtual void SetLaunchSwitches() OVERRIDE {
    UIPerfTest::SetLaunchSwitches();

    // Expose garbage collection for the page cycler tests.
    launch_arguments_.AppendSwitchASCII(switches::kJavaScriptFlags,
                                        "--expose_gc");
  }

  virtual base::FilePath GetDataPath(const char* name) {
    // Make sure the test data is checked out
    base::FilePath test_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("data"));
    test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  virtual bool HasErrors(const std::string /*timings*/) {
    return false;
  }

  virtual int GetTestIterations() {
    return num_test_iterations_;
  }

  virtual void GetTestUrl(const char* name, bool use_http, GURL *test_url) {
    base::FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory " << test_path.value();
    PopulateBufferCache(test_path);

    if (use_http) {
      *test_url = GURL(std::string(kBaseUrl) + name + "/start.html");
    } else {
      test_path = test_path.Append(FILE_PATH_LITERAL("start.html"));
      *test_url = net::FilePathToFileURL(test_path);
    }

    // run N iterations
    GURL::Replacements replacements;
    const std::string query_string =
        "iterations=" + base::IntToString(GetTestIterations()) + "&auto=1";
    replacements.SetQuery(
        query_string.c_str(),
        url_parse::Component(0, query_string.length()));
    *test_url = test_url->ReplaceComponents(replacements);
  }

  // For HTTP tests, the name must be safe for use in a URL without escaping.
  void RunPageCycler(const char* name, std::wstring* pages,
                     std::string* timings, bool use_http) {
    GURL test_url;
    GetTestUrl(name, use_http, &test_url);

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    ASSERT_EQ(AUTOMATION_MSG_NAVIGATION_SUCCESS, tab->NavigateToURL(test_url));

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilCookieValue(
        tab.get(), test_url, "__pc_done",
        TestTimeouts::large_test_timeout(), "1"));

    std::string cookie;
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_pages", &cookie));
    pages->assign(UTF8ToWide(cookie));
    ASSERT_FALSE(pages->empty());

    // Wait for the report.html to be loaded.
    ASSERT_TRUE(WaitUntilCookieValue(
        tab.get(), test_url, "__navigated_to_report",
        TestTimeouts::action_max_timeout(), "1"));

    // Get the timing cookie value from the DOM automation.
    std::wstring wcookie;
    ASSERT_TRUE(tab->ExecuteAndExtractString(L"",
          L"window.domAutomationController.send("
          L"JSON.stringify(__get_timings()));",
          &wcookie));
    cookie = base::SysWideToNativeMB(wcookie);

    // JSON.stringify() encapsulates the returned string in quotes, strip them.
    std::string::size_type start_idx = cookie.find("\"");
    std::string::size_type end_idx = cookie.find_last_of("\"");
    if (start_idx != std::string::npos &&
        end_idx != std::string::npos &&
        start_idx < end_idx) {
      cookie = cookie.substr(start_idx+1, end_idx-start_idx-1);
    }

    timings->assign(cookie);
    ASSERT_FALSE(timings->empty());
  }

  // When use_http is true, the test name passed here will be used directly in
  // the path to the test data, so it must be safe for use in a URL without
  // escaping. (No pound (#), question mark (?), semicolon (;), non-ASCII, or
  // other funny stuff.)
  void RunTestWithSuffix(const char* graph, const char* name, bool use_http,
                         const char* suffix) {
    std::wstring pages;
    std::string timings;
    size_t start_size = base::GetSystemCommitCharge();
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty() || HasErrors(timings))
      return;
    size_t stop_size = base::GetSystemCommitCharge();

    if (!print_times_only_) {
      PrintMemoryUsageInfo(suffix);
      PrintIOPerfInfo(suffix);
      perf_test::PrintSystemCommitCharge(suffix, stop_size - start_size,
                                         false /* not important */);
      ASSERT_NO_FATAL_FAILURE(PrintMemoryHistograms());
    }

    std::string trace_name = "t" + std::string(suffix);

    printf("Pages: [%s]\n", base::SysWideToNativeMB(pages).c_str());

    perf_test::PrintResultList(graph, "", trace_name, timings, "ms",
                               true /* important */);
  }

  void RunTest(const char* graph, const char* name, bool use_http) {
    RunTestWithSuffix(graph, name, use_http, "");
  }

 private:
  void PrintMemoryHistogram(const std::string& name,
                            const std::string& units) {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    std::wstring whistogram;
    ASSERT_TRUE(tab->ExecuteAndExtractString(
        L"",
        L"window.domAutomationController.send("
        L"window.domAutomationController.getHistogram ? "
        L"window.domAutomationController.getHistogram(\"" +
        base::SysUTF8ToWide(name) + L"\") : '')",
        &whistogram));
    std::string histogram = base::SysWideToNativeMB(whistogram);
    printf("HISTOGRAM %s: %s = %s %s\n",
           name.c_str(), name.c_str(), histogram.c_str(), units.c_str());
  }

  void PrintMemoryHistograms() {
    ASSERT_NO_FATAL_FAILURE(PrintMemoryHistogram(
        "V8.MemoryExternalFragmentationTotal", "percent"));
    ASSERT_NO_FATAL_FAILURE(PrintMemoryHistogram(
        "V8.MemoryHeapSampleTotalCommitted", "kb"));
    ASSERT_NO_FATAL_FAILURE(PrintMemoryHistogram(
        "V8.MemoryHeapSampleTotalUsed", "kb"));
  }

 protected:
  bool print_times_only_;

 private:
  int num_test_iterations_;
};

class PageCyclerReferenceTest : public PageCyclerTest {
 public:
  virtual void SetUp() {
    UseReferenceBuild();
    PageCyclerTest::SetUp();
  }

  void RunTest(const char* graph, const char* name, bool use_http) {
    // Run the test.
    PageCyclerTest::RunTestWithSuffix(graph, name, use_http, "_ref");
  }
};

class PageCyclerExtensionTest : public PageCyclerTest {
 public:
  // Note: we delay the SetUp until RunTest is called so that we can set
  // the user_data_dir based on the test name.
  virtual void SetUp() {}
  void RunTest(const char* graph, const char* extension_profile,
               const char* output_suffix, const char* name, bool use_http) {
    // Set up the extension profile directory.
    ASSERT_TRUE(extension_profile != NULL);
    base::FilePath data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
    data_dir = data_dir.AppendASCII("extensions").AppendASCII("profiles").
        AppendASCII(extension_profile);
    ASSERT_TRUE(file_util::DirectoryExists(data_dir));
    set_template_user_data(data_dir);

    // Now run the test.
    PageCyclerTest::SetUp();
    PageCyclerTest::RunTestWithSuffix(graph, name, use_http, output_suffix);
  }
};

class PageCyclerExtensionWebRequestTest : public PageCyclerExtensionTest {
 public:
  PageCyclerExtensionWebRequestTest()
      : PageCyclerExtensionTest()
  {
    // Enable experimental extension APIs for webrequest tests.
    launch_arguments_.AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

static base::FilePath GetDatabaseDataPath(const char* name) {
  base::FilePath test_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
  test_path = test_path.Append(FILE_PATH_LITERAL("tools"));
  test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
  test_path = test_path.Append(FILE_PATH_LITERAL("database"));
  test_path = test_path.AppendASCII(name);
  return test_path;
}

static base::FilePath GetIndexedDatabaseDataPath(const char* name) {
  base::FilePath test_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
  test_path = test_path.Append(FILE_PATH_LITERAL("tools"));
  test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
  test_path = test_path.Append(FILE_PATH_LITERAL("indexed_db"));
  test_path = test_path.AppendASCII(name);
  return test_path;
}

static bool HasDatabaseErrors(const std::string timings) {
  size_t pos = 0;
  size_t new_pos = 0;
  std::string time_str;
  int time = 0;
  do {
    new_pos = timings.find(',', pos);
    if (new_pos == std::string::npos)
      new_pos = timings.length();
    if (!base::StringToInt(base::StringPiece(timings.begin() + pos,
                                             timings.begin() + new_pos),
                           &time)) {
      LOG(ERROR) << "Invalid time reported: " << time_str;
      return true;
    }
    if (time < 0) {
      switch (time) {
        case -1:
          LOG(ERROR) << "Error while opening the database.";
          break;
        case -2:
          LOG(ERROR) << "Error while setting up the database.";
          break;
        case -3:
          LOG(ERROR) << "Error while running the transactions.";
          break;
        default:
          LOG(ERROR) << "Unknown error: " << time;
      }
      return true;
    }

    pos = new_pos + 1;
  } while (pos < timings.length());

  return false;
}

class PageCyclerDatabaseTest : public PageCyclerTest {
 public:
  PageCyclerDatabaseTest() {
    print_times_only_ = true;
  }

  virtual base::FilePath GetDataPath(const char* name) OVERRIDE {
    return GetDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) OVERRIDE {
    return HasDatabaseErrors(timings);
  }

  virtual int GetTestIterations() OVERRIDE {
    return kDatabaseTestIterations;
  }
};

class PageCyclerDatabaseReferenceTest : public PageCyclerReferenceTest {
 public:
  PageCyclerDatabaseReferenceTest() {
    print_times_only_ = true;
  }

  virtual base::FilePath GetDataPath(const char* name) OVERRIDE {
    return GetDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) OVERRIDE {
    return HasDatabaseErrors(timings);
  }

  virtual int GetTestIterations() OVERRIDE {
    return kDatabaseTestIterations;
  }
};

class PageCyclerIndexedDatabaseTest : public PageCyclerTest {
 public:
  PageCyclerIndexedDatabaseTest() {
    print_times_only_ = true;
  }

  virtual base::FilePath GetDataPath(const char* name) OVERRIDE {
    return GetIndexedDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) OVERRIDE {
    return HasDatabaseErrors(timings);
  }

  virtual int GetTestIterations() OVERRIDE {
    return kIDBTestIterations;
  }
};

class PageCyclerIndexedDatabaseReferenceTest : public PageCyclerReferenceTest {
 public:
  PageCyclerIndexedDatabaseReferenceTest() {
    print_times_only_ = true;
  }

  virtual base::FilePath GetDataPath(const char* name) OVERRIDE {
    return GetIndexedDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) OVERRIDE {
    return HasDatabaseErrors(timings);
  }

  virtual int GetTestIterations() OVERRIDE {
    return kIDBTestIterations;
  }
};

// Web Page Replay is a proxy server to record and serve pages
// with realistic network delays and bandwidth throttling.
// runtest.py launches replay.py to support these tests.
class PageCyclerWebPageReplayTest : public PageCyclerTest {
 public:
  PageCyclerWebPageReplayTest() {
    // These Chrome command-line arguments need to be kept in sync
    // with src/tools/python/google/webpagereplay_utils.py.
    base::FilePath extension_path = GetPageCyclerWprPath("extension");
    launch_arguments_.AppendSwitchPath(
        switches::kLoadExtension, extension_path);
    // TODO(slamm): Instead of kHostResolverRules, add a new switch,
    //     kTestingFixedDnsPort, and configure Web Page Replay to run
    //     a DNS proxy on that port to test Chrome's DNS code.
    launch_arguments_.AppendSwitchASCII(
        switches::kHostResolverRules, "MAP * 127.0.0.1");
    launch_arguments_.AppendSwitchASCII(
        switches::kTestingFixedHttpPort, kWebPageReplayHttpPort);
    launch_arguments_.AppendSwitchASCII(
        switches::kTestingFixedHttpsPort, kWebPageReplayHttpsPort);
    launch_arguments_.AppendSwitch(switches::kEnableExperimentalExtensionApis);
    launch_arguments_.AppendSwitch(switches::kEnableStatsTable);
    launch_arguments_.AppendSwitch(switches::kEnableBenchmarking);
    launch_arguments_.AppendSwitch(switches::kIgnoreCertificateErrors);
    launch_arguments_.AppendSwitch(switches::kNoProxyServer);
  }

  base::FilePath GetPageCyclerWprPath(const char* name) {
    base::FilePath wpr_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &wpr_path);
    wpr_path = wpr_path.AppendASCII("tools");
    wpr_path = wpr_path.AppendASCII("page_cycler");
    wpr_path = wpr_path.AppendASCII("webpagereplay");
    wpr_path = wpr_path.AppendASCII(name);
    return wpr_path;
  }

  virtual int GetTestIterations() OVERRIDE {
    return kWebPageReplayIterations;
  }

  virtual void GetTestUrl(const char* name, bool use_http,
                          GURL *test_url) OVERRIDE {
    base::FilePath start_path = GetPageCyclerWprPath("start.html");

    // Add query parameters for iterations and test name.
    const std::string query_string =
        "iterations=" + base::IntToString(GetTestIterations()) +
        "&test=" + name +
        "&auto=1";
    GURL::Replacements replacements;
    replacements.SetQuery(
        query_string.c_str(),
        url_parse::Component(0, query_string.length()));

    *test_url = net::FilePathToFileURL(start_path);
    *test_url = test_url->ReplaceComponents(replacements);
  }

  void RunTest(const char* graph, const char* name) {
    base::FilePath test_path = GetPageCyclerWprPath("tests");
    test_path = test_path.AppendASCII(name);
    test_path = test_path.ReplaceExtension(FILE_PATH_LITERAL(".js"));
    ASSERT_TRUE(file_util::PathExists(test_path))
        << "Missing test file " << test_path.value();

    const bool use_http = false;  // always use a file
    PageCyclerTest::RunTestWithSuffix(graph, name, use_http, "");
  }
};

// This macro simplifies setting up regular and reference build tests.
#define PAGE_CYCLER_TESTS(test, name, use_http) \
TEST_F(PageCyclerTest, name) { \
  RunTest("times", test, use_http); \
} \
TEST_F(PageCyclerReferenceTest, name) { \
  RunTest("times", test, use_http); \
}

// This macro simplifies setting up regular and reference build tests
// for HTML5 database tests.
#define PAGE_CYCLER_DATABASE_TESTS(test, name) \
TEST_F(PageCyclerDatabaseTest, Database##name##File) { \
  RunTest(test, test, false); \
} \
TEST_F(PageCyclerDatabaseReferenceTest, Database##name##File) { \
  RunTest(test, test, false); \
}

// This macro simplifies setting up regular and reference build tests
// for HTML5 Indexed DB tests.
#define PAGE_CYCLER_IDB_TESTS(test, name) \
TEST_F(PageCyclerIndexedDatabaseTest, IndexedDB##name##File) { \
  RunTest(test, test, false); \
} \
TEST_F(PageCyclerIndexedDatabaseReferenceTest, IndexedDB##name##File) { \
  RunTest(test, test, false); \
}

// These are shorthand for File vs. Http tests.
#define PAGE_CYCLER_FILE_TESTS(test, name) \
  PAGE_CYCLER_TESTS(test, name, false)
#define PAGE_CYCLER_HTTP_TESTS(test, name) \
  PAGE_CYCLER_TESTS(test, name, true)

// This macro lets us define tests with 1 and 10 extensions with 1 content
// script each. The name for the 10-extension case is changed so as not
// to run by default on the buildbots.
#define PAGE_CYCLER_EXTENSIONS_FILE_TESTS(test, name) \
TEST_F(PageCyclerExtensionTest, name) { \
  RunTest("times", "content_scripts1", "_extcs1", test, false); \
} \
TEST_F(PageCyclerExtensionTest, name##10) { \
  RunTest("times", "content_scripts10", "_extcs10", test, false); \
}

// This macro lets us define tests with an extension that listens to the
// webrequest.onBeforeRequest. It measures the effect that a blocking event
// for every request has on page cycle time.
#define PAGE_CYCLER_EXTENSIONS_WEBREQUEST_FILE_TESTS(test, name) \
TEST_F(PageCyclerExtensionWebRequestTest, name) { \
  RunTest("times", "extension_webrequest", "_extwr", test, false); \
}

#define PAGE_CYCLER_WEBPAGEREPLAY_TESTS(test, name) \
TEST_F(PageCyclerWebPageReplayTest, name) { \
  RunTest("times", test); \
}

// file-URL tests
PAGE_CYCLER_FILE_TESTS("moz", MozFile);
PAGE_CYCLER_EXTENSIONS_FILE_TESTS("moz", MozFile);
PAGE_CYCLER_EXTENSIONS_WEBREQUEST_FILE_TESTS("moz", MozFile)
PAGE_CYCLER_FILE_TESTS("intl1", Intl1File);
PAGE_CYCLER_FILE_TESTS("intl2", Intl2File);
PAGE_CYCLER_EXTENSIONS_WEBREQUEST_FILE_TESTS("intl2", Intl2File);
PAGE_CYCLER_FILE_TESTS("dom", DomFile);
PAGE_CYCLER_FILE_TESTS("dhtml", DhtmlFile);
PAGE_CYCLER_FILE_TESTS("morejs", MorejsFile);
PAGE_CYCLER_EXTENSIONS_FILE_TESTS("morejs", MorejsFile);
// added more tests here:
PAGE_CYCLER_FILE_TESTS("alexa_us", Alexa_usFile);
PAGE_CYCLER_FILE_TESTS("moz2", Moz2File);
PAGE_CYCLER_FILE_TESTS("morejsnp", MorejsnpFile);
PAGE_CYCLER_FILE_TESTS("bloat", BloatFile);

// http (localhost) tests
PAGE_CYCLER_HTTP_TESTS("moz", MozHttp);
PAGE_CYCLER_HTTP_TESTS("intl1", Intl1Http);
PAGE_CYCLER_HTTP_TESTS("intl2", Intl2Http);
PAGE_CYCLER_HTTP_TESTS("dom", DomHttp);
PAGE_CYCLER_HTTP_TESTS("bloat", BloatHttp);

// Web Page Replay (simulated network) tests.
// Windows is unsupported because of issues with loopback adapter and
// dummynet is unavailable on Vista and above.
#if !defined(OS_WIN)
PAGE_CYCLER_WEBPAGEREPLAY_TESTS("2012Q2", 2012Q2);
#endif

// HTML5 database tests
// These tests are _really_ slow on XP/Vista.
#if !defined(OS_WIN)
PAGE_CYCLER_DATABASE_TESTS("select-transactions",
                           SelectTransactions);
PAGE_CYCLER_DATABASE_TESTS("select-readtransactions",
                           SelectReadTransactions);
PAGE_CYCLER_DATABASE_TESTS("select-readtransactions-read-results",
                           SelectReadTransactionsReadResults);
PAGE_CYCLER_DATABASE_TESTS("insert-transactions",
                           InsertTransactions);
PAGE_CYCLER_DATABASE_TESTS("update-transactions",
                           UpdateTransactions);
PAGE_CYCLER_DATABASE_TESTS("delete-transactions",
                           DeleteTransactions);
PAGE_CYCLER_DATABASE_TESTS("pseudo-random-transactions",
                           PseudoRandomTransactions);
#endif

// Indexed DB tests.
PAGE_CYCLER_IDB_TESTS("basic_insert", BasicInsert);

}  // namespace
