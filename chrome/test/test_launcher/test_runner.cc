// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_launcher/test_runner.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/linked_ptr.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/test/test_suite.h"
#include "base/time.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";
const char kGTestFilterFlag[] = "gtest_filter";

// The default output file for XML output.
static const FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
  "test_detail.xml");

// A helper class to output results.
// Note: as currently XML is the only supported format by gtest, we don't
// check output format (e.g. "xml:" prefix) here and output an XML file
// unconditionally.
// Note: we don't output per-test-case or total summary info like
// total failed_test_count, disabled_test_count, elapsed_time and so on.
// Only each test (testcase element in the XML) will have the correct
// failed/disabled/elapsed_time information. Each test won't include
// detailed failure messages either.
class ResultsPrinter {
 public:
  explicit ResultsPrinter(const CommandLine& command_line);
  ~ResultsPrinter();
  void OnTestCaseStart(const char* name, int test_count) const;
  void OnTestCaseEnd() const;
  void OnTestEnd(const char* name, const char* case_name, bool run,
                 bool failed, bool failure_ignored, double elapsed_time) const;
 private:
  FILE* out_;
};

ResultsPrinter::ResultsPrinter(const CommandLine& command_line) : out_(NULL) {
  if (!command_line.HasSwitch(kGTestOutputFlag))
    return;
  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  FilePath path;
  if (colon_pos != std::string::npos) {
    FilePath flag_path = command_line.GetSwitchValuePath(kGTestOutputFlag);
    FilePath::StringType path_string = flag_path.value();
    path = FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (file_util::EndsWithSeparator(path)) {
      FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
          FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = FilePath(kDefaultOutputFile);
  FilePath dir_name = path.DirName();
  if (!file_util::DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value() << std::endl;
    // Create the directory if necessary (because the gtest does the same).
    file_util::CreateDirectory(dir_name);
  }
  out_ = file_util::OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << "." << std::endl;
    return;
  }
  fprintf(out_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_, "<testsuites name=\"AllTests\" tests=\"\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n");
}

ResultsPrinter::~ResultsPrinter() {
  if (!out_)
    return;
  fprintf(out_, "</testsuites>\n");
  fclose(out_);
}

void ResultsPrinter::OnTestCaseStart(const char* name, int test_count) const {
  if (!out_)
    return;
  fprintf(out_, "  <testsuite name=\"%s\" tests=\"%d\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n", name, test_count);
}

void ResultsPrinter::OnTestCaseEnd() const {
  if (!out_)
    return;
  fprintf(out_, "  </testsuite>\n");
}

void ResultsPrinter::OnTestEnd(const char* name, const char* case_name,
                               bool run, bool failed, bool failure_ignored,
                               double elapsed_time) const {
  if (!out_)
    return;
  fprintf(out_, "    <testcase name=\"%s\" status=\"%s\" time=\"%.3f\""
          " classname=\"%s\"",
          name, run ? "run" : "notrun", elapsed_time / 1000.0, case_name);
  if (!failed) {
    fprintf(out_, " />\n");
    return;
  }
  fprintf(out_, ">\n");
  fprintf(out_, "      <failure message=\"\" type=\"\"%s></failure>\n",
          failure_ignored ? " ignored=\"true\"" : "");
  fprintf(out_, "    </testcase>\n");
}

class TestCasePrinterHelper {
 public:
  TestCasePrinterHelper(const ResultsPrinter& printer, const char* name,
                        int total_test_count)
      : printer_(printer) {
    printer_.OnTestCaseStart(name, total_test_count);
  }
  ~TestCasePrinterHelper() {
    printer_.OnTestCaseEnd();
  }
 private:
  const ResultsPrinter& printer_;
};

// For a basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc, see the comment below and http://crbug.com/44497)
bool PatternMatchesString(const char *pattern, const char *str) {
  switch (*pattern) {
    case '\0':
    case ':':  // Either ':' or '\0' marks the end of the pattern.
      return *str == '\0';
    case '?':  // Matches any single character.
      return *str != '\0' && PatternMatchesString(pattern + 1, str + 1);
    case '*':  // Matches any string (possibly empty) of characters.
      return (*str != '\0' && PatternMatchesString(pattern, str + 1)) ||
          PatternMatchesString(pattern + 1, str);
    default:  // Non-special character.  Matches itself.
      return *pattern == *str &&
          PatternMatchesString(pattern + 1, str + 1);
  }
}

// TODO(phajdan.jr): Avoid duplicating gtest code. (http://crbug.com/44497)
// For basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc)
bool MatchesFilter(const std::string& name, const std::string& filter) {
  const char *cur_pattern = filter.c_str();
  for (;;) {
    if (PatternMatchesString(cur_pattern, name.c_str())) {
      return true;
    }

    // Finds the next pattern in the filter.
    cur_pattern = strchr(cur_pattern, ':');

    // Returns if no more pattern can be found.
    if (cur_pattern == NULL) {
      return false;
    }

    // Skips the pattern separater (the ':' character).
    cur_pattern++;
  }
}

}  // namespace

namespace tests {

TestRunner::TestRunner() {
}

TestRunner::~TestRunner() {
}

bool RunTests(const TestRunnerFactory& test_runner_factory) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  DCHECK(!command_line->HasSwitch(kGTestListTestsFlag));

  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::string filter_string = command_line->GetSwitchValueASCII(
      kGTestFilterFlag);

  int test_run_count = 0;
  int ignored_failure_count = 0;
  std::vector<std::string> failed_tests;

  ResultsPrinter printer(*command_line);
  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    TestCasePrinterHelper helper(printer, test_case->name(),
                                 test_case->total_test_count());
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      // Skip disabled tests.
      if (std::string(test_info->name()).find("DISABLED") == 0 &&
          !command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
        printer.OnTestEnd(test_info->name(), test_case->name(),
                          false, false, false, 0);
        continue;
      }
      std::string test_name = test_info->test_case_name();
      test_name.append(".");
      test_name.append(test_info->name());
      // Skip the test that doesn't match the filter string (if given).
      if (filter_string.size() && !MatchesFilter(test_name, filter_string)) {
        printer.OnTestEnd(test_info->name(), test_case->name(),
                          false, false, false, 0);
        continue;
      }
      base::Time start_time = base::Time::Now();
      scoped_ptr<TestRunner> test_runner(
          test_runner_factory.CreateTestRunner());
      if (!test_runner.get() || !test_runner->Init())
        return false;
      ++test_run_count;
      if (!test_runner->RunTest(test_name.c_str())) {
        failed_tests.push_back(test_name);
        bool ignore_failure = TestSuite::ShouldIgnoreFailure(*test_info);
        printer.OnTestEnd(test_info->name(), test_case->name(), true, true,
                          ignore_failure,
                          (base::Time::Now() - start_time).InMillisecondsF());
        if (ignore_failure)
          ++ignored_failure_count;
      } else {
        printer.OnTestEnd(test_info->name(), test_case->name(), true, false,
                          false,
                          (base::Time::Now() - start_time).InMillisecondsF());
      }
    }
  }

  printf("%d test%s run\n", test_run_count, test_run_count > 1 ? "s" : "");
  printf("%d test%s failed (%d ignored)\n",
         static_cast<int>(failed_tests.size()),
         failed_tests.size() > 1 ? "s" : "", ignored_failure_count);
  if (failed_tests.size() - ignored_failure_count == 0)
    return true;

  printf("Failing tests:\n");
  for (std::vector<std::string>::const_iterator iter = failed_tests.begin();
       iter != failed_tests.end(); ++iter) {
    printf("%s\n", iter->c_str());
  }

  return false;
}

}  // namespace
