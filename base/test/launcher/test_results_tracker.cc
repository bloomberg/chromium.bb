// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_results_tracker.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

namespace {

// The default output file for XML output.
const FilePath::CharType kDefaultOutputFile[] = FILE_PATH_LITERAL(
    "test_detail.xml");

}  // namespace

ResultsPrinter::ResultsPrinter(const CommandLine& command_line,
                               const TestsResultCallback& callback)
    : test_started_count_(0),
      test_run_count_(0),
      out_(NULL),
      callback_(callback),
      weak_ptr_(this) {
  if (!command_line.HasSwitch(kGTestOutputFlag))
    return;
  std::string flag = command_line.GetSwitchValueASCII(kGTestOutputFlag);
  size_t colon_pos = flag.find(':');
  FilePath path;
  if (colon_pos != std::string::npos) {
    FilePath flag_path =
        command_line.GetSwitchValuePath(kGTestOutputFlag);
    FilePath::StringType path_string = flag_path.value();
    path = FilePath(path_string.substr(colon_pos + 1));
    // If the given path ends with '/', consider it is a directory.
    // Note: This does NOT check that a directory (or file) actually exists
    // (the behavior is same as what gtest does).
    if (path.EndsWithSeparator()) {
      FilePath executable = command_line.GetProgram().BaseName();
      path = path.Append(executable.ReplaceExtension(
          FilePath::StringType(FILE_PATH_LITERAL("xml"))));
    }
  }
  if (path.value().empty())
    path = FilePath(kDefaultOutputFile);
  FilePath dir_name = path.DirName();
  if (!DirectoryExists(dir_name)) {
    LOG(WARNING) << "The output directory does not exist. "
                 << "Creating the directory: " << dir_name.value();
    // Create the directory if necessary (because the gtest does the same).
    file_util::CreateDirectory(dir_name);
  }
  out_ = file_util::OpenFile(path, "w");
  if (!out_) {
    LOG(ERROR) << "Cannot open output file: "
               << path.value() << ".";
    return;
  }
}

void ResultsPrinter::OnTestStarted(const std::string& name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ++test_started_count_;
}

void ResultsPrinter::OnAllTestsStarted() {
  if (test_started_count_ == 0) {
    fprintf(stdout, "0 tests run\n");
    fflush(stdout);

    // No tests have actually been started, so fire the callback
    // to avoid a hang.
    callback_.Run(true);
    delete this;
  }
}

void ResultsPrinter::AddTestResult(const TestResult& result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ++test_run_count_;
  results_[result.test_case_name].push_back(result);

  // TODO(phajdan.jr): Align counter (padding).
  std::string status_line(StringPrintf("[%" PRIuS "/%" PRIuS "] %s ",
                                       test_run_count_,
                                       test_started_count_,
                                       result.GetFullName().c_str()));
  if (result.completed()) {
    status_line.append(StringPrintf("(%" PRId64 " ms)",
                       result.elapsed_time.InMilliseconds()));
  } else if (result.status == TestResult::TEST_TIMEOUT) {
    status_line.append("(TIMED OUT)");
  } else if (result.status == TestResult::TEST_CRASH) {
    status_line.append("(CRASHED)");
  } else if (result.status == TestResult::TEST_SKIPPED) {
    status_line.append("(SKIPPED)");
  } else if (result.status == TestResult::TEST_UNKNOWN) {
    status_line.append("(UNKNOWN)");
  } else {
    // Fail very loudly so it's not ignored.
    CHECK(false) << "Unhandled test result status: " << result.status;
  }
  fprintf(stdout, "%s\n", status_line.c_str());
  fflush(stdout);

  tests_by_status_[result.status].push_back(result.GetFullName());

  if (test_run_count_ == test_started_count_) {
    fprintf(stdout, "%" PRIuS " test%s run\n",
            test_run_count_,
            test_run_count_ > 1 ? "s" : "");
    fflush(stdout);

    PrintTestsByStatus(TestResult::TEST_FAILURE, "failed");
    PrintTestsByStatus(TestResult::TEST_TIMEOUT, "timed out");
    PrintTestsByStatus(TestResult::TEST_CRASH, "crashed");
    PrintTestsByStatus(TestResult::TEST_SKIPPED, "skipped");
    PrintTestsByStatus(TestResult::TEST_UNKNOWN, "had unknown result");

    callback_.Run(
        tests_by_status_[TestResult::TEST_SUCCESS].size() == test_run_count_);

    delete this;
  }
}

WeakPtr<ResultsPrinter> ResultsPrinter::GetWeakPtr() {
  return weak_ptr_.GetWeakPtr();
}

ResultsPrinter::~ResultsPrinter() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!out_)
    return;
  fprintf(out_, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(out_, "<testsuites name=\"AllTests\" tests=\"\" failures=\"\""
          " disabled=\"\" errors=\"\" time=\"\">\n");
  for (ResultsMap::iterator i = results_.begin(); i != results_.end(); ++i) {
    fprintf(out_, "  <testsuite name=\"%s\" tests=\"%" PRIuS "\" failures=\"\""
            " disabled=\"\" errors=\"\" time=\"\">\n",
            i->first.c_str(), i->second.size());
    for (size_t j = 0; j < i->second.size(); ++j) {
      const TestResult& result = i->second[j];
      fprintf(out_, "    <testcase name=\"%s\" status=\"run\" time=\"%.3f\""
              " classname=\"%s\">\n",
              result.test_name.c_str(),
              result.elapsed_time.InSecondsF(),
              result.test_case_name.c_str());
      if (result.status != TestResult::TEST_SUCCESS)
        fprintf(out_, "      <failure message=\"\" type=\"\"></failure>\n");
      fprintf(out_, "    </testcase>\n");
    }
    fprintf(out_, "  </testsuite>\n");
  }
  fprintf(out_, "</testsuites>\n");
  fclose(out_);
}

void ResultsPrinter::PrintTestsByStatus(TestResult::Status status,
                                        const std::string& description) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::vector<std::string>& tests = tests_by_status_[status];
  if (tests.empty())
    return;
  fprintf(stdout,
          "%" PRIuS " test%s %s:\n",
          tests.size(),
          tests.size() != 1 ? "s" : "",
          description.c_str());
  for (size_t i = 0; i < tests.size(); i++)
    fprintf(stdout, "    %s\n", tests[i].c_str());
  fflush(stdout);
}

}  // namespace base
