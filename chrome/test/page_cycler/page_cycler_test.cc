// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/chrome_process_util.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"

#if defined(OS_MACOSX)
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/resource.h>
#endif

#ifndef NDEBUG
#define TEST_ITERATIONS "2"
#else
#define TEST_ITERATIONS "10"
#endif

// URL at which data files may be found for HTTP tests.  The document root of
// this URL's server should point to data/page_cycler/.
static const char kBaseUrl[] = "http://localhost:8000/";

namespace {

#if defined(OS_MACOSX)
// TODO(tvl/stuart): remove all this fd limit setting on the Mac when/if we
// replace the Mac reference build.  The trunk raises the limit within the
// browser app, but we do this here also so the current reference build without
// that code will pass the intl[12] tests.  This keeps us on an older
// reference build for all the other reference tests (javascript benchmarks,
// tab switch, etc.).
rlim_t GetFileDescriptorLimit(void) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    return limits.rlim_cur;
  }
  PLOG(ERROR) << "Failed to get file descriptor limit";
  return 0;
}

void SetFileDescriptorLimit(rlim_t max_descriptors) {
  struct rlimit limits;
  if (getrlimit(RLIMIT_NOFILE, &limits) == 0) {
    if (limits.rlim_max == 0) {
      limits.rlim_cur = max_descriptors;
    } else {
      limits.rlim_cur = std::min(max_descriptors, limits.rlim_max);
    }
    if (setrlimit(RLIMIT_NOFILE, &limits) != 0) {
      PLOG(ERROR) << "Failed to set file descriptor limit";
    }
  } else {
    PLOG(ERROR) << "Failed to get file descriptor limit";
  }
}

void PopulateUBC(const FilePath &test_dir) {
  // This will recursively walk the directory given and read all the files it
  // finds.  This is done so the Mac UBC is likely to have as much loaded as
  // possible.  Without this, the tests of this build gets one set of timings
  // and then the reference build test, gets slightly faster ones (even if the
  // reference build is the same binary).  The hope is by forcing all the
  // possible data into the UBC we equalize the tests for comparing timing data.

  // We don't want to walk into .svn dirs, so we have to do the tree walk
  // ourselves.

  std::vector<FilePath> dirs;
  dirs.push_back(test_dir);
  const FilePath svn_dir(FILE_PATH_LITERAL(".svn"));

  for (size_t idx = 0; idx < dirs.size(); ++idx) {
    file_util::FileEnumerator dir_enumerator(dirs[idx], false,
                                      file_util::FileEnumerator::DIRECTORIES);
    FilePath path;
    for (path = dir_enumerator.Next();
         !path.empty();
         path = dir_enumerator.Next()) {
      if (path.BaseName() != svn_dir)
        dirs.push_back(path);
    }
  }

  char buf[1024];
  unsigned int loaded = 0;

  // We seem to have some files in the data dirs that are just there for
  // reference, make a quick attempt to skip them by matching suffixes.
  std::vector<FilePath::StringType> ignore_suffixes;
  ignore_suffixes.push_back(FILE_PATH_LITERAL(".orig.html"));
  ignore_suffixes.push_back(FILE_PATH_LITERAL(".html-original"));

  std::vector<FilePath>::const_iterator iter;
  for (iter = dirs.begin(); iter != dirs.end(); ++iter) {
    file_util::FileEnumerator file_enumerator(*iter, false,
                                              file_util::FileEnumerator::FILES);
    FilePath path;
    for (path = file_enumerator.Next();
         !path.empty();
         path = file_enumerator.Next()) {
      const FilePath base_name = path.BaseName();
      const size_t base_name_size = base_name.value().size();

      bool should_skip = false;
      std::vector<FilePath::StringType>::const_iterator ignore_iter;
      for (ignore_iter = ignore_suffixes.begin();
           ignore_iter != ignore_suffixes.end();
           ++ignore_iter) {
        const FilePath::StringType &suffix = *ignore_iter;

        if ((base_name_size > suffix.size()) &&
            (base_name.value().compare(base_name_size - suffix.size(),
                                       suffix.size(), suffix) == 0)) {
          should_skip = true;
          break;
        }
      }
      if (should_skip)
        continue;

      // Read the file to get it into the UBC
      int fd = open(path.value().c_str(), O_RDONLY);
      if (fd >= 0) {
        ++loaded;
        while (HANDLE_EINTR(read(fd, buf, sizeof(buf))) > 0) {
        }
        HANDLE_EINTR(close(fd));
      }
    }
  }
  LOG(INFO) << "UBC should be loaded with " << loaded << " files.";
}
#endif  // defined(OS_MACOSX)

class PageCyclerTest : public UITest {
#if defined(OS_MACOSX)
 protected:
  rlim_t fd_limit_;
#endif
 public:
  PageCyclerTest() {
    show_window_ = true;

    // Expose garbage collection for the page cycler tests.
    launch_arguments_.AppendSwitchWithValue(switches::kJavaScriptFlags,
                                            L"--expose_gc");
#if defined(OS_MACOSX)
    static rlim_t initial_fd_limit = GetFileDescriptorLimit();
    fd_limit_ = initial_fd_limit;
#endif
  }

  void SetUp() {
#if defined(OS_MACOSX)
    SetFileDescriptorLimit(fd_limit_);
#endif
    UITest::SetUp();
  }

  virtual FilePath GetDataPath(const char* name) {
    // Make sure the test data is checked out
    FilePath test_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
    test_path = test_path.Append(FILE_PATH_LITERAL("data"));
    test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
    test_path = test_path.AppendASCII(name);
    return test_path;
  }

  virtual bool HasErrors(const std::string /*timings*/) {
    return false;
  }

  // For HTTP tests, the name must be safe for use in a URL without escaping.
  void RunPageCycler(const char* name, std::wstring* pages,
                     std::string* timings, bool use_http) {
    FilePath test_path = GetDataPath(name);
    ASSERT_TRUE(file_util::DirectoryExists(test_path))
        << "Missing test directory " << test_path.value();

#if defined(OS_MACOSX)
    PopulateUBC(test_path);
#endif

    GURL test_url;
    if (use_http) {
      test_url = GURL(std::string(kBaseUrl) + name + "/start.html");
    } else {
      test_path = test_path.Append(FILE_PATH_LITERAL("start.html"));
      test_url = net::FilePathToFileURL(test_path);
    }

    // run N iterations
    GURL::Replacements replacements;
    const char query_string[] = "iterations=" TEST_ITERATIONS "&auto=1";
    replacements.SetQuery(
        query_string,
        url_parse::Component(0, arraysize(query_string) - 1));
    test_url = test_url.ReplaceComponents(replacements);

    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());
    tab->NavigateToURL(test_url);

    // Wait for the test to finish.
    ASSERT_TRUE(WaitUntilCookieValue(tab.get(), test_url, "__pc_done",
                                     3000, UITest::test_timeout_ms(), "1"));

    std::string cookie;
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_pages", &cookie));
    pages->assign(UTF8ToWide(cookie));
    ASSERT_FALSE(pages->empty());
    ASSERT_TRUE(tab->GetCookieByName(test_url, "__pc_timings", &cookie));
    timings->assign(cookie);
    ASSERT_FALSE(timings->empty());
  }

  // When use_http is true, the test name passed here will be used directly in
  // the path to the test data, so it must be safe for use in a URL without
  // escaping. (No pound (#), question mark (?), semicolon (;), non-ASCII, or
  // other funny stuff.)
  void RunTestWithSuffix(const char* name, bool use_http, const char* suffix) {
    std::wstring pages;
    std::string timings;
    size_t start_size = base::GetSystemCommitCharge();
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty() || HasErrors(timings))
      return;
    size_t stop_size = base::GetSystemCommitCharge();

    FilePath data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &data_dir);

    PrintMemoryUsageInfo(suffix, data_dir);
    PrintIOPerfInfo(suffix, data_dir);
    PrintSystemCommitCharge(suffix, stop_size - start_size,
                            false /* not important */);

    std::string trace_name = "t" + std::string(suffix);
    wprintf(L"\nPages: [%ls]\n", pages.c_str());
    PrintResultList("times", "", trace_name, timings, "ms",
                    true /* important */);
  }

  void RunTest(const char* name, bool use_http) {
    RunTestWithSuffix(name, use_http, "");
  }
};

class PageCyclerReferenceTest : public PageCyclerTest {
 public:
  // override the browser directory that is used by UITest::SetUp to cause it
  // to use the reference build instead.
  void SetUp() {
#if defined(OS_MACOSX)
    fd_limit_ = 1024;
#endif

    FilePath dir;
    PathService::Get(chrome::DIR_TEST_TOOLS, &dir);
    dir = dir.AppendASCII("reference_build");
#if defined(OS_WIN)
    dir = dir.AppendASCII("chrome");
#elif defined(OS_LINUX)
    dir = dir.AppendASCII("chrome_linux");
#elif defined(OS_MACOSX)
    dir = dir.AppendASCII("chrome_mac");
#endif
    browser_directory_ = dir;
    PageCyclerTest::SetUp();
  }

  void RunTest(const char* name, bool use_http) {
    std::wstring pages;
    std::string timings;
    size_t start_size = base::GetSystemCommitCharge();
    RunPageCycler(name, &pages, &timings, use_http);
    if (timings.empty())
      return;
    size_t stop_size = base::GetSystemCommitCharge();

    FilePath data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &data_dir);

    PrintMemoryUsageInfo("_ref", data_dir);
    PrintIOPerfInfo("_ref", data_dir);
    PrintSystemCommitCharge("_ref", stop_size - start_size,
                            false /* not important */);

    PrintResultList("times", "", "t_ref", timings, "ms",
                    true /* important */);
  }
};

class PageCyclerExtensionTest : public PageCyclerTest {
 public:
  void SetUp() {}
  void RunTest(const char* extension_profile, const char* output_suffix,
               const char* name, bool use_http) {
    // Set up the extension profile directory.
    ASSERT_TRUE(extension_profile != NULL);
    FilePath data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &data_dir);
    data_dir = data_dir.AppendASCII("extensions").AppendASCII("profiles").
        AppendASCII(extension_profile);
    ASSERT_TRUE(file_util::DirectoryExists(data_dir));
    set_template_user_data(data_dir);

    // Now run the test.
    PageCyclerTest::SetUp();
    PageCyclerTest::RunTestWithSuffix(name, use_http, output_suffix);
  }
};

static FilePath GetDatabaseDataPath(const char* name) {
  FilePath test_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &test_path);
  test_path = test_path.Append(FILE_PATH_LITERAL("tools"));
  test_path = test_path.Append(FILE_PATH_LITERAL("page_cycler"));
  test_path = test_path.Append(FILE_PATH_LITERAL("database"));
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
    time_str = timings.substr(pos, new_pos - pos);
    if (!StringToInt(time_str, &time)) {
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
  virtual FilePath GetDataPath(const char* name) {
    return GetDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) {
    return HasDatabaseErrors(timings);
  }
};

class PageCyclerDatabaseReferenceTest : public PageCyclerReferenceTest {
 public:
  virtual FilePath GetDataPath(const char* name) {
    return GetDatabaseDataPath(name);
  }

  virtual bool HasErrors(const std::string timings) {
    return HasDatabaseErrors(timings);
  }
};

// This macro simplifies setting up regular and reference build tests.
#define PAGE_CYCLER_TESTS(test, name, use_http) \
TEST_F(PageCyclerTest, name) { \
  RunTest(test, use_http); \
} \
TEST_F(PageCyclerReferenceTest, name) { \
  RunTest(test, use_http); \
}

// This macro simplifies setting up regular and reference build tests
// for HTML5 database tests.
#define PAGE_CYCLER_DATABASE_TESTS(test, name) \
TEST_F(PageCyclerDatabaseTest, name) { \
  RunTest(test, false); \
} \
TEST_F(PageCyclerDatabaseReferenceTest, name) { \
  RunTest(test, false); \
}

// These are shorthand for File vs. Http tests.
#define PAGE_CYCLER_FILE_TESTS(test, name)\
  PAGE_CYCLER_TESTS(test, name, false)
#define PAGE_CYCLER_HTTP_TESTS(test, name)\
  PAGE_CYCLER_TESTS(test, name, true)

// This macro lets us define tests with 1 and 10 extensions with 1 content
// script each. The name for the 10-extension case is changed so as not
// to run by default on the buildbots.
#define PAGE_CYCLER_EXTENSIONS_FILE_TESTS(test, name) \
TEST_F(PageCyclerExtensionTest, name) {  \
  RunTest("content_scripts1", "_extcs1", test, false); \
} \
TEST_F(PageCyclerExtensionTest, name##10) { \
  RunTest("content_scripts10", "_extcs10", test, false); \
}

// file-URL tests
PAGE_CYCLER_FILE_TESTS("moz", MozFile);
PAGE_CYCLER_EXTENSIONS_FILE_TESTS("moz", MozFile);
PAGE_CYCLER_FILE_TESTS("intl1", Intl1File);
PAGE_CYCLER_FILE_TESTS("intl2", Intl2File);
PAGE_CYCLER_FILE_TESTS("dom", DomFile);
PAGE_CYCLER_FILE_TESTS("dhtml", DhtmlFile);
PAGE_CYCLER_FILE_TESTS("morejs", MorejsFile);
PAGE_CYCLER_EXTENSIONS_FILE_TESTS("morejs", MorejsFile);

// http (localhost) tests
PAGE_CYCLER_HTTP_TESTS("moz", MozHttp);
PAGE_CYCLER_HTTP_TESTS("intl1", Intl1Http);
PAGE_CYCLER_HTTP_TESTS("intl2", Intl2Http);
PAGE_CYCLER_HTTP_TESTS("dom", DomHttp);
PAGE_CYCLER_HTTP_TESTS("bloat", BloatHttp);

// HTML5 database tests
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

}  // namespace
