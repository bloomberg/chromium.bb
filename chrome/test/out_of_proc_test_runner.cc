// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/linked_ptr.h"
#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/test/test_suite.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/unit/chrome_test_suite.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/base_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/sandbox_policy.h"
#include "sandbox/src/dep.h"
#include "sandbox/src/sandbox_factory.h"
#include "sandbox/src/sandbox_types.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
// The entry point signature of chrome.dll.
typedef int (*DLL_MAIN)(HINSTANCE, sandbox::SandboxInterfaceInfo*, wchar_t*);
#endif  // defined(OS_WIN)

// Allows a specific test to increase its timeout.  This should be used very
// sparingly, i.e. when a "test" is a really a collection of sub tests and it's
// not possible to break them up.  An example is PDFBrowserTest.Loading, which
// loads all the files it can find in a directory.
base::hash_map<std::string, int> g_test_timeout_overrides;

namespace {

const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";

const char kSingleProcessTestsFlag[]   = "single_process";
const char kSingleProcessTestsAndChromeFlag[]   = "single-process";
// The following is kept for historical reasons (so people that are used to
// using it don't get surprised).
const char kChildProcessFlag[]   = "child";

const char kHelpFlag[]   = "help";

const char kTestTerminateTimeoutFlag[] = "test-terminate-timeout";

// How long we wait for the subprocess to exit (with a success/failure code).
// See http://crbug.com/43862 for some discussion of the value.
const int kDefaultTestTimeoutMs = 20000;

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

  // TODO(phajdan.jr): Convert bool failed, bool failure_ignored to an enum.
  void OnTestEnd(const char* name, const char* case_name, bool run,
                 bool failed, bool failure_ignored, double elapsed_time) const;
 private:
  FILE* out_;

  DISALLOW_COPY_AND_ASSIGN(ResultsPrinter);
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

void ResultsPrinter::OnTestEnd(const char* name,
                               const char* case_name,
                               bool run,
                               bool failed,
                               bool failure_ignored,
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
  TestCasePrinterHelper(const ResultsPrinter& printer,
                        const char* name,
                        int total_test_count)
      : printer_(printer) {
    printer_.OnTestCaseStart(name, total_test_count);
  }
  ~TestCasePrinterHelper() {
    printer_.OnTestCaseEnd();
  }
 private:
  const ResultsPrinter& printer_;

  DISALLOW_COPY_AND_ASSIGN(TestCasePrinterHelper);
};

// For a basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc, see the comment below and http://crbug.com/44497)
bool PatternMatchesString(const char* pattern, const char* str) {
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

// Runs test specified by |test_name| in a child process,
// and returns the exit code.
int RunTest(const std::string& test_name) {
  // Some of the below method calls will leak objects if there is no
  // autorelease pool in place.
  base::mac::ScopedNSAutoreleasePool pool;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  CommandLine new_cmd_line(cmd_line->GetProgram());
  CommandLine::SwitchMap switches = cmd_line->GetSwitches();

  // Strip out gtest_output flag because otherwise we would overwrite results
  // of the previous test. We will generate the final output file later
  // in RunTests().
  switches.erase(kGTestOutputFlag);

  // Strip out gtest_repeat flag because we can only run one test in the child
  // process (restarting the browser in the same process is illegal after it
  // has been shut down and will actually crash).
  switches.erase(kGTestRepeatFlag);

  // Strip out user-data-dir if present.  We will add it back in again later.
  switches.erase(switches::kUserDataDir);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_cmd_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Always enable disabled tests.  This method is not called with disabled
  // tests unless this flag was specified to the browser test executable.
  new_cmd_line.AppendSwitch("gtest_also_run_disabled_tests");
  new_cmd_line.AppendSwitchASCII("gtest_filter", test_name);
  new_cmd_line.AppendSwitch(kChildProcessFlag);

  // Do not let the child ignore failures.  We need to propagate the
  // failure status back to the parent.
  new_cmd_line.AppendSwitch(base::TestSuite::kStrictFailureHandling);

  // Create a new user data dir and pass it to the child.
  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir() || !temp_dir.IsValid()) {
    LOG(ERROR) << "Error creating temp profile directory";
    return false;
  }
  new_cmd_line.AppendSwitchPath(switches::kUserDataDir, temp_dir.path());

  // file:// access for ChromeOS.
  new_cmd_line.AppendSwitch(switches::kAllowFileAccess);

  base::ProcessHandle process_handle;
#if defined(OS_POSIX)
  const char* browser_wrapper = getenv("BROWSER_WRAPPER");
  if (browser_wrapper) {
    new_cmd_line.PrependWrapper(browser_wrapper);
    VLOG(1) << "BROWSER_WRAPPER was set, prefixing command_line with "
            << browser_wrapper;
  }

  // On POSIX, we launch the test in a new process group with pgid equal to
  // its pid. Any child processes that the test may create will inherit the
  // same pgid. This way, if the test is abruptly terminated, we can clean up
  // any orphaned child processes it may have left behind.
  base::environment_vector no_env;
  base::file_handle_mapping_vector no_files;
  if (!base::LaunchAppInNewProcessGroup(new_cmd_line.argv(), no_env, no_files,
                                        false, &process_handle))
    return false;
#else
  if (!base::LaunchApp(new_cmd_line, false, false, &process_handle))
    return false;
#endif

  int test_terminate_timeout_ms = kDefaultTestTimeoutMs;
  if (g_test_timeout_overrides.count(test_name))
    test_terminate_timeout_ms = g_test_timeout_overrides[test_name];
  if (cmd_line->HasSwitch(kTestTerminateTimeoutFlag)) {
    std::string timeout_str =
        cmd_line->GetSwitchValueASCII(kTestTerminateTimeoutFlag);
    int timeout;
    base::StringToInt(timeout_str, &timeout);
    test_terminate_timeout_ms = std::max(test_terminate_timeout_ms, timeout);
  }

  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle, &exit_code,
                                        test_terminate_timeout_ms)) {
    LOG(ERROR) << "Test timeout (" << test_terminate_timeout_ms
               << " ms) exceeded for " << test_name;

    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    // Ensure that the process terminates.
    base::KillProcess(process_handle, -1, true);
  }

#if defined(OS_POSIX)
  if (exit_code != 0) {
    // On POSIX, in case the test does not exit cleanly, either due to a crash
    // or due to it timing out, we need to clean up any child processes that
    // it might have created. On Windows, child processes are automatically
    // cleaned up using JobObjects.
    base::KillProcessGroup(process_handle);
  }
#endif

  return exit_code;
}

bool RunTests() {
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
      ++test_run_count;
      int exit_code = RunTest(test_name);
      if (exit_code == 0) {
        // Test passed.
        printer.OnTestEnd(test_info->name(), test_case->name(), true, false,
                          false,
                          (base::Time::Now() - start_time).InMillisecondsF());
      } else {
        failed_tests.push_back(test_name);

        bool ignore_failure = false;

        // -1 exit code means a crash or hang. Never ignore those failures,
        // they are serious and should always be visible.
        if (exit_code != -1)
          ignore_failure = base::TestSuite::ShouldIgnoreFailure(*test_info);

        printer.OnTestEnd(test_info->name(), test_case->name(), true, true,
                          ignore_failure,
                          (base::Time::Now() - start_time).InMillisecondsF());
        if (ignore_failure)
          ++ignored_failure_count;
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

void PrintUsage() {
  fprintf(stdout,
      "Runs tests using the gtest framework, each test being run in its own\n"
      "process.  Any gtest flags can be specified.\n"
      "  --single_process\n"
      "    Runs the tests and the launcher in the same process. Useful for \n"
      "    debugging a specific test in a debugger.\n"
      "  --single-process\n"
      "    Same as above, and also runs Chrome in single-process mode.\n"
      "  --test-terminate-timeout\n"
      "    Specifies a timeout (in milliseconds) after which a running test\n"
      "    will be forcefully terminated.\n"
      "  --help\n"
      "    Shows this message.\n"
      "  --gtest_help\n"
      "    Shows the gtest help message.\n");
}

}  // namespace

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kHelpFlag)) {
    PrintUsage();
    return 0;
  }

  // TODO(pkasting): This "single_process vs. single-process" design is terrible
  // UI.  Instead, there should be some sort of signal flag on the command line,
  // with all subsequent arguments passed through to the underlying browser.
  if (command_line->HasSwitch(kChildProcessFlag) ||
      command_line->HasSwitch(kSingleProcessTestsFlag) ||
      command_line->HasSwitch(kSingleProcessTestsAndChromeFlag) ||
      command_line->HasSwitch(kGTestListTestsFlag) ||
      command_line->HasSwitch(kGTestHelpFlag)) {

#if defined(OS_WIN)
    if (command_line->HasSwitch(kChildProcessFlag) ||
        command_line->HasSwitch(kSingleProcessTestsFlag)) {
      // This is the browser process, so setup the sandbox broker.
      sandbox::BrokerServices* broker_services =
          sandbox::SandboxFactory::GetBrokerServices();
      if (broker_services) {
        sandbox::InitBrokerServices(broker_services);
        // Precreate the desktop and window station used by the renderers.
        sandbox::TargetPolicy* policy = broker_services->CreatePolicy();
        sandbox::ResultCode result = policy->CreateAlternateDesktop(true);
        CHECK(sandbox::SBOX_ERROR_FAILED_TO_SWITCH_BACK_WINSTATION != result);
        policy->Release();
      }
    }
#endif
    return ChromeTestSuite(argc, argv).Run();
  }

#if defined(OS_WIN)
  if (command_line->HasSwitch(switches::kProcessType)) {
    // This is a child process, call ChromeMain.
    FilePath chrome_path(command_line->GetProgram().DirName());
    chrome_path = chrome_path.Append(chrome::kBrowserResourcesDll);
    HMODULE dll = LoadLibrary(chrome_path.value().c_str());
    DLL_MAIN entry_point =
        reinterpret_cast<DLL_MAIN>(::GetProcAddress(dll, "ChromeMain"));
    if (!entry_point)
      return -1;

    // Initialize the sandbox services.
    sandbox::SandboxInterfaceInfo sandbox_info = {0};
    sandbox_info.target_services = sandbox::SandboxFactory::GetTargetServices();
    return entry_point(GetModuleHandle(NULL), &sandbox_info, GetCommandLineW());
  }
#endif

  fprintf(stdout,
      "Starting tests...\n"
      "IMPORTANT DEBUGGING NOTE: each test is run inside its own process.\n"
      "For debugging a test inside a debugger, use the\n"
      "--gtest_filter=<your_test_name> flag along with either\n"
      "--single_process (to run all tests in one launcher/browser process) or\n"
      "--single-process (to do the above, and also run Chrome in single-\n"
      "process mode).\n");

  int cycles = 1;
  if (command_line->HasSwitch(kGTestRepeatFlag)) {
    base::StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag),
                      &cycles);
  }

  int exit_code = 0;
  while (cycles != 0) {
    if (!RunTests()) {
      exit_code = 1;
      break;
    }

    // Special value "-1" means "repeat indefinitely".
    if (cycles != -1)
      cycles--;
  }
  return exit_code;
}
