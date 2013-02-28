// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_cycler/page_cycler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"

// Basic PageCyclerBrowserTest structure; used in testing most of PageCycler's
// functionality.
class PageCyclerBrowserTest : public content::NotificationObserver,
                              public InProcessBrowserTest {
 public:
  PageCyclerBrowserTest() : page_cycler_(NULL) {
  }

  virtual ~PageCyclerBrowserTest() {
  }

  // Initialize file paths within a temporary directory; this should be
  // empty and nonexistent.
  virtual void InitFilePaths(base::FilePath temp_path) {
    temp_path_ = temp_path;
    urls_file_ = temp_path.AppendASCII("urls_file");
    errors_file_ = temp_path.AppendASCII("errors");
    stats_file_ = temp_path.AppendASCII("stats");

    ASSERT_FALSE(file_util::PathExists(urls_file_));
    ASSERT_FALSE(file_util::PathExists(errors_file_));
    ASSERT_FALSE(file_util::PathExists(stats_file_));
  }

  // Initialize a PageCycler using either the base fields, or using provided
  // ones.
  void InitPageCycler() {
    page_cycler_ = new PageCycler(browser(), urls_file());
    page_cycler_->set_errors_file(errors_file());
    page_cycler_->set_stats_file(stats_file());
  }

  void InitPageCycler(base::FilePath urls_file,
                      base::FilePath errors_file,
                      base::FilePath stats_file) {
    page_cycler_ = new PageCycler(browser(), urls_file);
    page_cycler_->set_errors_file(errors_file);
    page_cycler_->set_stats_file(stats_file);
  }

  void RegisterForNotifications() {
    registrar_.Add(this, chrome::NOTIFICATION_BROWSER_CLOSED,
                   content::NotificationService::AllSources());
  }

  // Get a collection of basic urls which are stored in the test directory.
  // NOTE: |test_server| must be started first!
  std::vector<GURL> GetURLs() {
    std::vector<GURL> urls;
    urls.push_back(test_server()->GetURL("files/page_cycler/basic_html.html"));
    urls.push_back(test_server()->GetURL("files/page_cycler/basic_js.html"));
    urls.push_back(test_server()->GetURL("files/page_cycler/basic_css.html"));
    return urls;
  }

  // Read the errors file, and generate a vector of error strings.
  std::vector<std::string> GetErrorsFromFile() {
    std::string error_file_contents;
    CHECK(file_util::ReadFileToString(errors_file_,
                                      &error_file_contents));
    if (error_file_contents[error_file_contents.size() - 1] == '\n')
      error_file_contents.resize(error_file_contents.size() - 1);

    std::vector<std::string> errors;
    base::SplitString(error_file_contents, '\n', &errors);

    return errors;
  }

  // Convert a vector of GURLs into a newline-separated string, ready to be
  // written to the urls file for PageCycler to use.
  std::string GetStringFromURLs(std::vector<GURL> urls) {
    std::string urls_string;
    for (std::vector<GURL>::const_iterator iter = urls.begin();
         iter != urls.end(); ++iter)
      urls_string.append(iter->spec() + "\n");
    return urls_string;
  }

  // content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    switch (type) {
      case chrome::NOTIFICATION_BROWSER_CLOSED:
        MessageLoop::current()->PostTask(FROM_HERE, MessageLoop::QuitClosure());
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  base::FilePath urls_file() { return urls_file_; }
  base::FilePath errors_file() { return errors_file_; }
  base::FilePath stats_file() { return stats_file_; }
  PageCycler* page_cycler() { return page_cycler_; }

 protected:
  base::FilePath temp_path_;
  base::FilePath urls_file_;
  base::FilePath errors_file_;
  base::FilePath stats_file_;
  PageCycler* page_cycler_;
  content::NotificationRegistrar registrar_;
};

// Structure used for testing PageCycler's ability to playback a series of
// URLs given a cache directory.
class PageCyclerCachedBrowserTest : public PageCyclerBrowserTest {
 public:
  // For a cached test, we use the provided user data directory from the test
  // directory.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    base::FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    test_dir = test_dir.AppendASCII("page_cycler");

    base::FilePath source_data_dir = test_dir.AppendASCII("cached_data_dir");
    CHECK(file_util::PathExists(source_data_dir));

    CHECK(user_data_dir_.CreateUniqueTempDir());

    base::FilePath dest_data_dir =
        user_data_dir_.path().AppendASCII("cached_data_dir");
    CHECK(!file_util::PathExists(dest_data_dir));

    CHECK(file_util::CopyDirectory(source_data_dir,
                                   user_data_dir_.path(),
                                   true));  // recursive.
    CHECK(file_util::PathExists(dest_data_dir));

    command_line->AppendSwitchPath(switches::kUserDataDir,
                                   dest_data_dir);
    command_line->AppendSwitch(switches::kPlaybackMode);
  }

  // Initialize the file paths to use the UserDataDir's urls file, instead
  // of one to be written.
  virtual void InitFilePaths(base::FilePath temp_path) OVERRIDE {
    urls_file_ = user_data_dir_.path().AppendASCII("cached_data_dir")
                                      .AppendASCII("urls");
    errors_file_ = temp_path.AppendASCII("errors");
    stats_file_ = temp_path.AppendASCII("stats");

    ASSERT_TRUE(file_util::PathExists(urls_file_));
    ASSERT_FALSE(file_util::PathExists(errors_file_));
    ASSERT_FALSE(file_util::PathExists(stats_file_));
  }

 private:
  // The directory storing the copy of the UserDataDir.
  base::ScopedTempDir user_data_dir_;
};

// Sanity check; iterate through a series of URLs and make sure there are no
// errors.
IN_PROC_BROWSER_TEST_F(PageCyclerBrowserTest, BasicTest) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  ASSERT_TRUE(test_server()->Start());

  std::string urls_string = GetStringFromURLs(GetURLs());;

  ASSERT_TRUE(file_util::WriteFile(urls_file(), urls_string.c_str(),
                                   urls_string.size()));

  InitPageCycler();
  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_FALSE(file_util::PathExists(errors_file()));
  ASSERT_TRUE(file_util::PathExists(stats_file()));
}

// Test to make sure that PageCycler will recognize unvisitable URLs, and will
// handle them appropriately.
IN_PROC_BROWSER_TEST_F(PageCyclerBrowserTest, UnvisitableURL) {
  const size_t kNumErrors = 1;
  const char kFakeURL[] = "http://www.pleasenoonehavethisurlanytimeinthenext"
                          "century.com/gibberish";
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  ASSERT_TRUE(test_server()->Start());

  std::vector<GURL> urls = GetURLs();
  urls.push_back(GURL(kFakeURL));
  std::string urls_string = GetStringFromURLs(urls);

  ASSERT_TRUE(file_util::WriteFile(urls_file(), urls_string.c_str(),
                                   urls_string.size()));

  InitPageCycler();
  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_TRUE(file_util::PathExists(errors_file()));
  ASSERT_TRUE(file_util::PathExists(stats_file()));

  std::vector<std::string> errors = GetErrorsFromFile();

  ASSERT_EQ(kNumErrors, errors.size());

  // Check that each error message contains the fake URL (i.e., that it wasn't
  // from a valid URL, and that the fake URL was caught each time).
  ASSERT_NE(std::string::npos, errors[0].find(kFakeURL));
}

// Test that PageCycler will remove an invalid URL prior to running.
IN_PROC_BROWSER_TEST_F(PageCyclerBrowserTest, InvalidURL) {
  const char kBadURL[] = "notarealurl";

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  ASSERT_TRUE(test_server()->Start());

  std::string urls_string = GetStringFromURLs(GetURLs());
  urls_string.append(kBadURL).append("\n");

  ASSERT_TRUE(file_util::WriteFile(urls_file(), urls_string.c_str(),
                                   urls_string.size()));

  InitPageCycler();
  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_TRUE(file_util::PathExists(errors_file()));
  ASSERT_TRUE(file_util::PathExists(stats_file()));

  std::vector<std::string> errors = GetErrorsFromFile();
  ASSERT_EQ(1u, errors.size());

  std::string expected_error = "Omitting invalid URL: ";
  expected_error.append(kBadURL).append(".");

  ASSERT_FALSE(errors[0].compare(expected_error));
}

// Test that PageCycler will remove a Chrome Error URL prior to running.
IN_PROC_BROWSER_TEST_F(PageCyclerBrowserTest, ChromeErrorURL) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  ASSERT_TRUE(test_server()->Start());

  std::vector<GURL> urls = GetURLs();
  urls.push_back(GURL(content::kUnreachableWebDataURL));
  std::string urls_string = GetStringFromURLs(urls);

  ASSERT_TRUE(file_util::WriteFile(urls_file(), urls_string.c_str(),
                                   urls_string.size()));

  InitPageCycler();
  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_TRUE(file_util::PathExists(errors_file()));
  ASSERT_TRUE(file_util::PathExists(stats_file()));

  std::vector<std::string> errors = GetErrorsFromFile();
  ASSERT_EQ(1u, errors.size());

  std::string expected_error = "Chrome error pages are not allowed as urls. "
                               "Omitting url: ";
  expected_error.append(content::kUnreachableWebDataURL).append(".");

  ASSERT_FALSE(errors[0].compare(expected_error));
}

#if !defined(OS_CHROMEOS)
// TODO(rdevlin.cronin): Perhaps page cycler isn't completely implemented on
// ChromeOS?

// Test that PageCycler will visit all the urls from a cache directory
// successfully while in playback mode.
#if defined(OS_LINUX)
// Bug 159026: Fails on Linux in both debug and release mode.
#define MAYBE_PlaybackMode DISABLED_PlaybackMode
#elif (defined(OS_WIN) || defined(OS_MACOSX) ) && !defined(NDEBUG)
// Bug 131333: This test fails on a XP debug bot since Build 17609.
#define MAYBE_PlaybackMode DISABLED_PlaybackMode
#else
#define MAYBE_PlaybackMode PlaybackMode
#endif
IN_PROC_BROWSER_TEST_F(PageCyclerCachedBrowserTest, MAYBE_PlaybackMode) {
  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  InitPageCycler();

  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_TRUE(file_util::PathExists(stats_file()));
  ASSERT_FALSE(file_util::PathExists(errors_file()));
}
#endif  // !defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
// TODO(rdevlin.cronin): Perhaps page cycler isn't completely implemented on
// ChromeOS?

// Test that PageCycler will have a cache miss if a URL is missing from the
// cache directory while in playback mode.
// Bug 131333: This test fails on a XP debug bot since Build 17609.
#if (defined(OS_WIN) || defined(OS_MACOSX)) && !defined(NDEBUG)
#define MAYBE_URLNotInCache DISABLED_URLNotInCache
#else
#define MAYBE_URLNotInCache URLNotInCache
#endif
IN_PROC_BROWSER_TEST_F(PageCyclerCachedBrowserTest, MAYBE_URLNotInCache) {
  const char kCacheMissURL[] = "http://www.images.google.com/";

  base::ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  RegisterForNotifications();
  InitFilePaths(temp.path());

  // Only use a single URL that is not in cache. That's sufficient for the test
  // scenario, and makes things faster than needlessly cycling through all the
  // other URLs.

  base::FilePath new_urls_file = temp.path().AppendASCII("urls");
  ASSERT_FALSE(file_util::PathExists(new_urls_file));

  ASSERT_TRUE(file_util::WriteFile(new_urls_file, kCacheMissURL,
                                   sizeof(kCacheMissURL)));

  InitPageCycler(new_urls_file, errors_file(), stats_file());
  page_cycler()->Run();

  content::RunMessageLoop();
  ASSERT_TRUE(file_util::PathExists(errors_file()));
  ASSERT_TRUE(file_util::PathExists(stats_file()));

  std::vector<std::string> errors = GetErrorsFromFile();
  ASSERT_EQ(1u, errors.size());

  std::string expected_error;
  expected_error.append("Failed to load the page at: ")
      .append(kCacheMissURL)
      .append(": The requested entry was not found in the cache.");

  ASSERT_FALSE(errors[0].compare(expected_error));
}
#endif  // !defined(OS_CHROMEOS)
