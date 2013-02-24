// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/record/record_api.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/experimental_record.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// Dummy content for mock stats file.
const std::string kTestStatistics = "Sample Stat 1\nSample Stat 2\n";

// Standard capture parameters, with a mix of good and bad URLs, and
// a hole for filling in the user data dir.

// Restore these on the next CL of the new page cycler, when the C++
// side's implementation is de-hacked.
//const char kCaptureArgs1[] =
//    "[\"%s\", [\"URL 1\", \"URL 2(bad)\", \"URL 3\", \"URL 4(bad)\"]]";

const char kCaptureArgs1[] =
    "[\"%s\", [\"http://www.google.com\", \"http://www.amazon.com\"]]";

// Standard playback parameters, with the same mix of good and bad URLs
// as the capture parameters, a hole for filling in the user data dir, and
// a mocked-up extension path and repeat count (which are used only to
// verify that they made it into the CommandLine, since extension loading
// and repeat-counting are hard to emulate in the test ProcessStrategy.
const char kPlaybackArgs1[] =
    "[\"%s\", 2, {\"extensionPath\": \"MockExtension\"}]";

// Use this as the value of base::FilePath switches (e.g. user-data-dir) that
// should be replaced by the record methods.
const base::FilePath::CharType kDummyDirName[] = FILE_PATH_LITERAL("ReplaceMe");

// Use this as the filename for a mock "cache" file in the user-data-dir.
const base::FilePath::CharType kMockCacheFile[] =
    FILE_PATH_LITERAL("MockCache");

}

class TestProcessStrategy : public ProcessStrategy {
 public:
  explicit TestProcessStrategy(std::vector<base::FilePath>* temp_files)
      : command_line_(CommandLine::NO_PROGRAM), temp_files_(temp_files) {}

  virtual ~TestProcessStrategy() {}

  // Pump the blocking pool queue, since this is needed during test.
  virtual void PumpBlockingPool() OVERRIDE {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
  }

  // Act somewhat like a real test browser instance.  In particular:
  // 1. If visit-urls, then
  //   a. If record-mode, then put a copy of the URL list into the user data
  //      directory in a mocked up cache file
  //   b. If playback-mode, then check for existence of that URL list copy
  //      in the cache
  //   c. Scan list of URLS, noting in |urls_visited_| all those
  //      visited.  If there are any "bad" URLS, don't visit these, but
  //      create a ".errors" file listing them.
  // 2. If record-stats, then create a mock stats file.
  virtual void RunProcess(const CommandLine& command_line,
                          std::vector<std::string>* errors) OVERRIDE {
    command_line_ = command_line;
    visited_urls_.clear();

    if (command_line.HasSwitch(switches::kVisitURLs)) {
      base::FilePath url_path =
          command_line.GetSwitchValuePath(switches::kVisitURLs);

      temp_files_->push_back(url_path);
      if (command_line.HasSwitch(switches::kRecordMode) ||
          command_line.HasSwitch(switches::kPlaybackMode)) {
        base::FilePath url_path_copy = command_line.GetSwitchValuePath(
            switches::kUserDataDir).Append(
            base::FilePath(base::FilePath::StringType(kMockCacheFile)));

        if (command_line.HasSwitch(switches::kRecordMode)) {
          file_util::CopyFile(url_path, url_path_copy);
        } else {
          if (!file_util::ContentsEqual(url_path, url_path_copy)) {
            std::string contents1, contents2;
            file_util::ReadFileToString(url_path, &contents1);
            file_util::ReadFileToString(url_path_copy, &contents2);
            LOG(ERROR) << "FILE MISMATCH" << contents1 << " VS " << contents2;
          }
          EXPECT_TRUE(file_util::ContentsEqual(url_path, url_path_copy));
        }
      }

      std::string urls;
      file_util::ReadFileToString(url_path, &urls);

      std::vector<std::string> url_vector, bad_urls;
      base::SplitString(urls, '\n', &url_vector);
      for (std::vector<std::string>::iterator itr = url_vector.begin();
          itr != url_vector.end(); ++itr) {
        if (itr->find_first_of("bad") != std::string::npos)
          bad_urls.push_back(*itr);
        else
          visited_urls_.push_back(*itr);
      }

      if (!bad_urls.empty()) {
        base::FilePath url_errors_path = url_path.DirName()
            .Append(url_path.BaseName().value() +
            base::FilePath::StringType(kURLErrorsSuffix));
        std::string error_content = JoinString(bad_urls, '\n');
        temp_files_->push_back(url_errors_path);
        file_util::WriteFile(url_errors_path, error_content.c_str(),
            error_content.size());
      }
    }

    if (command_line.HasSwitch(switches::kRecordStats)) {
      base::FilePath record_stats_path(command_line.GetSwitchValuePath(
          switches::kRecordStats));
      temp_files_->push_back(record_stats_path);
      file_util::WriteFile(record_stats_path, kTestStatistics.c_str(),
          kTestStatistics.size());
    }
  }

  const CommandLine& GetCommandLine() const {
    return command_line_;
  }

  const std::vector<std::string>& GetVisitedURLs() const {
    return visited_urls_;
  }

 private:
  CommandLine command_line_;
  std::vector<std::string> visited_urls_;
  std::vector<base::FilePath>* temp_files_;
};

class RecordApiTest : public InProcessBrowserTest {
 public:
  RecordApiTest() {}
  virtual ~RecordApiTest() {}

  // Override to scope known temp directories outside the scope of the running
  // browser test.
  virtual void SetUp() OVERRIDE {
    InProcessBrowserTest::SetUp();
    if (!scoped_temp_user_data_dir_.Set(base::FilePath(kDummyDirName)))
      NOTREACHED();
  }

  // Override to delete temp directories.
  virtual void TearDown() OVERRIDE {
    if (!scoped_temp_user_data_dir_.Delete())
      NOTREACHED();
    InProcessBrowserTest::TearDown();
  }

  // Override to delete temporary files created during execution.
  virtual void CleanUpOnMainThread() OVERRIDE {
    InProcessBrowserTest::CleanUpOnMainThread();
    for (std::vector<base::FilePath>::const_iterator it = temp_files_.begin();
        it != temp_files_.end(); ++it) {
      if (!file_util::Delete(*it, false))
        NOTREACHED();
    }
  }

  // Override SetUpCommandline to specify a dummy user_data_dir, which
  // should be replaced.  Clear record-mode, playback-mode, visit-urls,
  // record-stats, and load-extension.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    std::vector<std::string> remove_switches;

    remove_switches.push_back(switches::kUserDataDir);
    remove_switches.push_back(switches::kVisitURLs);
    remove_switches.push_back(switches::kPlaybackMode);
    remove_switches.push_back(switches::kRecordStats);
    remove_switches.push_back(switches::kLoadExtension);
    *command_line = RunPageCyclerFunction::RemoveSwitches(*command_line,
        remove_switches);

    command_line->AppendSwitchPath(switches::kUserDataDir,
        base::FilePath(kDummyDirName));
    // Adding a dummy load-extension switch is rather complex since the
    // preent design of InProcessBrowserTest requires a *real* extension
    // for the flag, even if we're just testing its replacement.  Opted
    // to omit this for the sake of simplicity.
  }

  // Run a capture, using standard URL test list and the specified
  // user data dir.  Return via |out_list| the list of error URLs,
  // if any, resulting from the capture.  And return directly the
  // RecordCaptureURLsFunction that was used, so that its state may be
  // queried.
  scoped_refptr<RecordCaptureURLsFunction> RunCapture(
      const base::FilePath& user_data_dir,
      scoped_ptr<base::ListValue>* out_list) {

    scoped_refptr<RecordCaptureURLsFunction> capture_function(
        new RecordCaptureURLsFunction(new TestProcessStrategy(&temp_files_)));

    std::string escaped_user_data_dir;
    ReplaceChars(user_data_dir.AsUTF8Unsafe(), "\\", "\\\\",
        &escaped_user_data_dir);

    out_list->reset(utils::ToList(
        utils::RunFunctionAndReturnSingleResult(capture_function.get(),
        base::StringPrintf(kCaptureArgs1, escaped_user_data_dir.c_str()),
        browser())));

    return capture_function;
  }

  // Verify that the URL list of good and bad URLs was properly handled.
  // Needed by several tests.
  bool VerifyURLHandling(const ListValue* result,
      const TestProcessStrategy& strategy) {

    // Check that the two bad URLs are returned.
    StringValue badURL2("URL 2(bad)"), badURL4("URL 4(bad)");

    /* TODO(CAS) Need to rework this once the new record API is implemented.
    const base::Value* string_value = NULL;
    EXPECT_TRUE(result->GetSize() == 2);
    result->Get(0, &string_value);
    EXPECT_TRUE(base::Value::Equals(string_value, &badURL2));
    result->Get(1, &string_value);
    EXPECT_TRUE(base::Value::Equals(string_value, &badURL4));

    // Check that both good URLs were visited.
    std::string goodURL1("URL 1"), goodURL3("URL 3");
    EXPECT_TRUE(strategy.GetVisitedURLs()[0].compare(goodURL1) == 0
        && strategy.GetVisitedURLs()[1].compare(goodURL3) == 0);
    */

    return true;
  }

 protected:
  std::vector<base::FilePath> temp_files_;

 private:
  base::ScopedTempDir scoped_temp_user_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(RecordApiTest);
};


IN_PROC_BROWSER_TEST_F(RecordApiTest, DISABLED_CheckCapture) {
  base::ScopedTempDir user_data_dir;
  scoped_ptr<base::ListValue> result;

  EXPECT_TRUE(user_data_dir.CreateUniqueTempDir());
  scoped_refptr<RecordCaptureURLsFunction> capture_URLs_function =
      RunCapture(user_data_dir.path(), &result);

  // Check that user-data-dir switch has been properly overridden.
  const TestProcessStrategy &strategy =
      static_cast<const TestProcessStrategy &>(
      capture_URLs_function->GetProcessStrategy());
  const CommandLine& command_line = strategy.GetCommandLine();

  EXPECT_TRUE(command_line.HasSwitch(switches::kUserDataDir));
  EXPECT_TRUE(command_line.GetSwitchValuePath(switches::kUserDataDir) !=
      base::FilePath(kDummyDirName));

  EXPECT_TRUE(VerifyURLHandling(result.get(), strategy));
}

#if defined(ADDRESS_SANITIZER)
// Times out under ASan, see http://crbug.com/130267.
#define MAYBE_CheckPlayback DISABLED_CheckPlayback
#else
// Flaky on all platforms, see http://crbug.com/167143
#define MAYBE_CheckPlayback DISABLED_CheckPlayback
#endif
IN_PROC_BROWSER_TEST_F(RecordApiTest, MAYBE_CheckPlayback) {
  base::ScopedTempDir user_data_dir;

  EXPECT_TRUE(user_data_dir.CreateUniqueTempDir());

  // Assume this RunCapture operates w/o error, since it's tested
  // elsewhere.
  scoped_ptr<base::ListValue> capture_result;
  RunCapture(user_data_dir.path(), &capture_result);

  std::string escaped_user_data_dir;
  ReplaceChars(user_data_dir.path().AsUTF8Unsafe(), "\\", "\\\\",
      &escaped_user_data_dir);

  scoped_refptr<RecordReplayURLsFunction> playback_function(
      new RecordReplayURLsFunction(
      new TestProcessStrategy(&temp_files_)));
  scoped_ptr<base::DictionaryValue> result(utils::ToDictionary(
      utils::RunFunctionAndReturnSingleResult(playback_function,
      base::StringPrintf(kPlaybackArgs1, escaped_user_data_dir.c_str()),
      browser())));

  // Check that command line user-data-dir was overridden.  (That
  // it was *consistently* overridden in both capture and replay
  // is verified elsewhere.
  const TestProcessStrategy &strategy =
      static_cast<const TestProcessStrategy &>(
      playback_function->GetProcessStrategy());
  const CommandLine& command_line = strategy.GetCommandLine();

  EXPECT_TRUE(command_line.HasSwitch(switches::kUserDataDir));
  EXPECT_TRUE(command_line.GetSwitchValuePath(switches::kUserDataDir) !=
      base::FilePath(kDummyDirName));

   // Check that command line load-extension was overridden.
  EXPECT_TRUE(command_line.HasSwitch(switches::kLoadExtension) &&
      command_line.GetSwitchValuePath(switches::kLoadExtension)
      != base::FilePath(kDummyDirName));

   // Check for return value with proper stats.
  EXPECT_EQ(kTestStatistics, utils::GetString(result.get(), kStatsKey));

  ListValue* errors = NULL;
  EXPECT_TRUE(result->GetList(kErrorsKey, &errors));
  EXPECT_TRUE(VerifyURLHandling(errors, strategy));
}

} // namespace extensions
