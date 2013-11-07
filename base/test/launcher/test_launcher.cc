// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

namespace {

// Maximum time of no output after which we print list of processes still
// running. This deliberately doesn't use TestTimeouts (which is otherwise
// a recommended solution), because they can be increased. This would defeat
// the purpose of this timeout, which is 1) to avoid buildbot "no output for
// X seconds" timeout killing the process 2) help communicate status of
// the test launcher to people looking at the output (no output for a long
// time is mysterious and gives no info about what is happening) 3) help
// debugging in case the process hangs anyway.
const int kOutputTimeoutSeconds = 15;

// Set of live launch test processes with corresponding lock (it is allowed
// for callers to launch processes on different threads).
LazyInstance<std::map<ProcessHandle, CommandLine> > g_live_processes
    = LAZY_INSTANCE_INITIALIZER;
LazyInstance<Lock> g_live_processes_lock = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_POSIX)
// Self-pipe that makes it possible to do complex shutdown handling
// outside of the signal handler.
int g_shutdown_pipe[2] = { -1, -1 };

void ShutdownPipeSignalHandler(int signal) {
  HANDLE_EINTR(write(g_shutdown_pipe[1], "q", 1));
}

void KillSpawnedTestProcesses() {
  // Keep the lock until exiting the process to prevent further processes
  // from being spawned.
  AutoLock lock(g_live_processes_lock.Get());

  fprintf(stdout,
          "Sending SIGTERM to %" PRIuS " child processes... ",
          g_live_processes.Get().size());
  fflush(stdout);

  for (std::map<ProcessHandle, CommandLine>::iterator i =
           g_live_processes.Get().begin();
       i != g_live_processes.Get().end();
       ++i) {
    // Send the signal to entire process group.
    kill((-1) * (i->first), SIGTERM);
  }

  fprintf(stdout,
          "done.\nGiving processes a chance to terminate cleanly... ");
  fflush(stdout);

  PlatformThread::Sleep(TimeDelta::FromMilliseconds(500));

  fprintf(stdout, "done.\n");
  fflush(stdout);

  fprintf(stdout,
          "Sending SIGKILL to %" PRIuS " child processes... ",
          g_live_processes.Get().size());
  fflush(stdout);

  for (std::map<ProcessHandle, CommandLine>::iterator i =
           g_live_processes.Get().begin();
       i != g_live_processes.Get().end();
       ++i) {
    // Send the signal to entire process group.
    kill((-1) * (i->first), SIGKILL);
  }

  fprintf(stdout, "done.\n");
  fflush(stdout);
}

// I/O watcher for the reading end of the self-pipe above.
// Terminates any launched child processes and exits the process.
class SignalFDWatcher : public MessageLoopForIO::Watcher {
 public:
  SignalFDWatcher() {
  }

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    fprintf(stdout, "\nCaught signal. Killing spawned test processes...\n");
    fflush(stdout);

    KillSpawnedTestProcesses();

    // The signal would normally kill the process, so exit now.
    exit(1);
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalFDWatcher);
};
#endif  // defined(OS_POSIX)

// Parses the environment variable var as an Int32.  If it is unset, returns
// true.  If it is set, unsets it then converts it to Int32 before
// returning it in |result|.  Returns true on success.
bool TakeInt32FromEnvironment(const char* const var, int32* result) {
  scoped_ptr<Environment> env(Environment::Create());
  std::string str_val;

  if (!env->GetVar(var, &str_val))
    return true;

  if (!env->UnSetVar(var)) {
    LOG(ERROR) << "Invalid environment: we could not unset " << var << ".\n";
    return false;
  }

  if (!StringToInt(str_val, result)) {
    LOG(ERROR) << "Invalid environment: " << var << " is not an integer.\n";
    return false;
  }

  return true;
}

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

void RunCallback(
    const TestLauncher::LaunchChildGTestProcessCallback& callback,
    int exit_code,
    const TimeDelta& elapsed_time,
    bool was_timeout,
    const std::string& output) {
  callback.Run(exit_code, elapsed_time, was_timeout, output);
}

void DoLaunchChildTestProcess(
    const CommandLine& command_line,
    base::TimeDelta timeout,
    scoped_refptr<MessageLoopProxy> message_loop_proxy,
    const TestLauncher::LaunchChildGTestProcessCallback& callback) {
  TimeTicks start_time = TimeTicks::Now();

  // Redirect child process output to a file.
  base::FilePath output_file;
  CHECK(file_util::CreateTemporaryFile(&output_file));

  LaunchOptions options;
#if defined(OS_WIN)
  // Make the file handle inheritable by the child.
  SECURITY_ATTRIBUTES sa_attr;
  sa_attr.nLength = sizeof(SECURITY_ATTRIBUTES);
  sa_attr.lpSecurityDescriptor = NULL;
  sa_attr.bInheritHandle = TRUE;

  win::ScopedHandle handle(CreateFile(output_file.value().c_str(),
                                      GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_DELETE,
                                      &sa_attr,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_TEMPORARY,
                                      NULL));
  CHECK(handle.IsValid());
  options.inherit_handles = true;
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = handle.Get();
  options.stderr_handle = handle.Get();
#elif defined(OS_POSIX)
  options.new_process_group = true;

  int output_file_fd = open(output_file.value().c_str(), O_RDWR);
  CHECK_GE(output_file_fd, 0);

  file_util::ScopedFD output_file_fd_closer(&output_file_fd);

  base::FileHandleMappingVector fds_mapping;
  fds_mapping.push_back(std::make_pair(output_file_fd, STDOUT_FILENO));
  fds_mapping.push_back(std::make_pair(output_file_fd, STDERR_FILENO));
  options.fds_to_remap = &fds_mapping;
#endif

  bool was_timeout = false;
  int exit_code = LaunchChildTestProcessWithOptions(
      command_line, options, timeout, &was_timeout);

#if defined(OS_WIN)
  FlushFileBuffers(handle.Get());
  handle.Close();
#elif defined(OS_POSIX)
  output_file_fd_closer.reset();
#endif

  std::string output_file_contents;
  CHECK(base::ReadFileToString(output_file, &output_file_contents));

  if (!base::DeleteFile(output_file, false)) {
    // This needs to be non-fatal at least for Windows.
    LOG(WARNING) << "Failed to delete " << output_file.AsUTF8Unsafe();
  }

  // Run target callback on the thread it was originating from, not on
  // a worker pool thread.
  message_loop_proxy->PostTask(
      FROM_HERE,
      Bind(&RunCallback,
           callback,
           exit_code,
           TimeTicks::Now() - start_time,
           was_timeout,
           output_file_contents));
}

}  // namespace

const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";

TestLauncherDelegate::~TestLauncherDelegate() {
}

TestLauncher::TestLauncher(TestLauncherDelegate* launcher_delegate,
                           size_t parallel_jobs)
    : launcher_delegate_(launcher_delegate),
      total_shards_(1),
      shard_index_(0),
      cycles_(1),
      test_started_count_(0),
      test_finished_count_(0),
      test_success_count_(0),
      test_broken_count_(0),
      retry_count_(0),
      retry_limit_(3),  // TODO(phajdan.jr): Make a flag control this.
      run_result_(true),
      watchdog_timer_(FROM_HERE,
                      TimeDelta::FromSeconds(kOutputTimeoutSeconds),
                      this,
                      &TestLauncher::OnOutputTimeout),
      worker_pool_owner_(
          new SequencedWorkerPoolOwner(parallel_jobs, "test_launcher")) {
}

TestLauncher::~TestLauncher() {
  worker_pool_owner_->pool()->Shutdown();
}

bool TestLauncher::Run(int argc, char** argv) {
  if (!Init())
    return false;

#if defined(OS_POSIX)
  CHECK_EQ(0, pipe(g_shutdown_pipe));

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = &ShutdownPipeSignalHandler;

  CHECK_EQ(0, sigaction(SIGINT, &action, NULL));
  CHECK_EQ(0, sigaction(SIGQUIT, &action, NULL));
  CHECK_EQ(0, sigaction(SIGTERM, &action, NULL));

  MessageLoopForIO::FileDescriptorWatcher controller;
  SignalFDWatcher watcher;

  CHECK(MessageLoopForIO::current()->WatchFileDescriptor(
            g_shutdown_pipe[0],
            true,
            MessageLoopForIO::WATCH_READ,
            &controller,
            &watcher));
#endif  // defined(OS_POSIX)

  // Start the watchdog timer.
  watchdog_timer_.Reset();

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&TestLauncher::RunTestIteration, Unretained(this)));

  MessageLoop::current()->Run();

  if (cycles_ != 1)
    results_tracker_.PrintSummaryOfAllIterations();

  MaybeSaveSummaryAsJSON();

  return run_result_;
}

void TestLauncher::LaunchChildGTestProcess(
    const CommandLine& command_line,
    const std::string& wrapper,
    base::TimeDelta timeout,
    const LaunchChildGTestProcessCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Record the exact command line used to launch the child.
  CommandLine new_command_line(
      PrepareCommandLineForGTest(command_line, wrapper));

  worker_pool_owner_->pool()->PostWorkerTask(
      FROM_HERE,
      Bind(&DoLaunchChildTestProcess,
           new_command_line,
           timeout,
           MessageLoopProxy::current(),
           Bind(&TestLauncher::OnLaunchTestProcessFinished,
                Unretained(this),
                callback)));
}

void TestLauncher::OnTestFinished(const TestResult& result) {
  ++test_finished_count_;

  bool print_snippet = false;
  std::string print_test_stdio("auto");
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestLauncherPrintTestStdio)) {
    print_test_stdio = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kTestLauncherPrintTestStdio);
  }
  if (print_test_stdio == "auto") {
    print_snippet = (result.status != TestResult::TEST_SUCCESS);
  } else if (print_test_stdio == "always") {
    print_snippet = true;
  } else if (print_test_stdio == "never") {
    print_snippet = false;
  } else {
    LOG(WARNING) << "Invalid value of " << switches::kTestLauncherPrintTestStdio
                 << ": " << print_test_stdio;
  }
  if (print_snippet) {
    fprintf(stdout, "%s", result.output_snippet.c_str());
    fflush(stdout);
  }

  if (result.status == TestResult::TEST_SUCCESS) {
    ++test_success_count_;
  } else {
    tests_to_retry_.insert(result.full_name);
  }

  results_tracker_.AddTestResult(result);

  // TODO(phajdan.jr): Align counter (padding).
  std::string status_line(
      StringPrintf("[%" PRIuS "/%" PRIuS "] %s ",
                   test_finished_count_,
                   test_started_count_,
                   result.full_name.c_str()));
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

  // We just printed a status line, reset the watchdog timer.
  watchdog_timer_.Reset();

  if (test_finished_count_ == test_started_count_) {
    if (!tests_to_retry_.empty() && retry_count_ < retry_limit_) {
      retry_count_++;

      std::vector<std::string> test_names(tests_to_retry_.begin(),
                                          tests_to_retry_.end());

      tests_to_retry_.clear();

      size_t retry_started_count =
          launcher_delegate_->RetryTests(this, test_names);
      if (retry_started_count == 0) {
        // Signal failure, but continue to run all requested test iterations.
        // With the summary of all iterations at the end this is a good default.
        run_result_ = false;

        // The current iteration is done.
        fprintf(stdout, "%" PRIuS " test%s run\n",
                test_finished_count_,
                test_finished_count_ > 1 ? "s" : "");
        fflush(stdout);

        results_tracker_.PrintSummaryOfCurrentIteration();

        // Kick off the next iteration.
        MessageLoop::current()->PostTask(
            FROM_HERE,
            Bind(&TestLauncher::RunTestIteration, Unretained(this)));
      } else {
        fprintf(stdout, "Retrying %" PRIuS " test%s (retry #%" PRIuS ")\n",
                retry_started_count,
                retry_started_count > 1 ? "s" : "",
                retry_count_);
        fflush(stdout);

        test_started_count_ += retry_started_count;
      }
    } else {
      // The current iteration is done.
      fprintf(stdout, "%" PRIuS " test%s run\n",
              test_finished_count_,
              test_finished_count_ > 1 ? "s" : "");
      fflush(stdout);

      results_tracker_.PrintSummaryOfCurrentIteration();

      // When we retry tests, success is determined by having nothing more
      // to retry (everything eventually passed), as opposed to having
      // no failures at all.
      if (!tests_to_retry_.empty()) {
        // Signal failure, but continue to run all requested test iterations.
        // With the summary of all iterations at the end this is a good default.
        run_result_ = false;
      }

      // Kick off the next iteration.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          Bind(&TestLauncher::RunTestIteration, Unretained(this)));
    }
  }

  // Do not waste time on timeouts. We include tests with unknown results here
  // because sometimes (e.g. hang in between unit tests) that's how a timeout
  // gets reported.
  if (result.status == TestResult::TEST_TIMEOUT ||
      result.status == TestResult::TEST_UNKNOWN) {
    test_broken_count_++;
  }
  size_t broken_threshold =
      std::max(static_cast<size_t>(10), test_started_count_ / 10);
  if (test_broken_count_ >= broken_threshold) {
    fprintf(stdout, "Too many badly broken tests (%" PRIuS "), exiting now.\n",
            test_broken_count_);
    fflush(stdout);

#if defined(OS_POSIX)
    KillSpawnedTestProcesses();
#endif  // defined(OS_POSIX)

    results_tracker_.AddGlobalTag("BROKEN_TEST_EARLY_EXIT");
    MaybeSaveSummaryAsJSON();

    exit(1);
  }
}

bool TestLauncher::Init() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(kGTestListTestsFlag)) {
    // Child gtest processes would list tests instead of running tests.
    // TODO(phajdan.jr): Restore support for the flag.
    LOG(ERROR) << kGTestListTestsFlag << " is not supported.";
    return false;
  }

  // Initialize sharding. Command line takes precedence over legacy environment
  // variables.
  if (command_line->HasSwitch(switches::kTestLauncherTotalShards) &&
      command_line->HasSwitch(switches::kTestLauncherShardIndex)) {
    if (!StringToInt(
            command_line->GetSwitchValueASCII(
                switches::kTestLauncherTotalShards),
            &total_shards_)) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherTotalShards;
      return false;
    }
    if (!StringToInt(
            command_line->GetSwitchValueASCII(
                switches::kTestLauncherShardIndex),
            &shard_index_)) {
      LOG(ERROR) << "Invalid value for " << switches::kTestLauncherShardIndex;
      return false;
    }
    fprintf(stdout,
            "Using sharding settings from command line. This is shard %d/%d\n",
            shard_index_, total_shards_);
    fflush(stdout);
  } else {
    if (!TakeInt32FromEnvironment(kTestTotalShards, &total_shards_))
      return false;
    if (!TakeInt32FromEnvironment(kTestShardIndex, &shard_index_))
      return false;
    fprintf(stdout,
            "Using sharding settings from environment. This is shard %d/%d\n",
            shard_index_, total_shards_);
    fflush(stdout);
  }
  if (shard_index_ < 0 ||
      total_shards_ < 0 ||
      shard_index_ >= total_shards_) {
    LOG(ERROR) << "Invalid sharding settings: we require 0 <= "
               << kTestShardIndex << " < " << kTestTotalShards
               << ", but you have " << kTestShardIndex << "=" << shard_index_
               << ", " << kTestTotalShards << "=" << total_shards_ << ".\n";
    return false;
  }

  if (command_line->HasSwitch(kGTestRepeatFlag) &&
      !StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag),
                   &cycles_)) {
    LOG(ERROR) << "Invalid value for " << kGTestRepeatFlag;
    return false;
  }

  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions.
  std::string filter = command_line->GetSwitchValueASCII(kGTestFilterFlag);
  positive_test_filter_ = filter;
  size_t dash_pos = filter.find('-');
  if (dash_pos != std::string::npos) {
    // Everything up to the dash.
    positive_test_filter_ = filter.substr(0, dash_pos);

    // Everything after the dash.
    negative_test_filter_ = filter.substr(dash_pos + 1);
  }

  if (!results_tracker_.Init(*command_line)) {
    LOG(ERROR) << "Failed to initialize test results tracker.";
    return 1;
  }

  return true;
}

void TestLauncher::RunTests() {
  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  int num_runnable_tests = 0;

  std::vector<std::string> test_names;

  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      std::string test_name = test_info->test_case_name();
      test_name.append(".");
      test_name.append(test_info->name());

      // Skip disabled tests.
      const CommandLine* command_line = CommandLine::ForCurrentProcess();
      if (test_name.find("DISABLED") != std::string::npos &&
          !command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
        continue;
      }

      std::string filtering_test_name =
          launcher_delegate_->GetTestNameForFiltering(test_case, test_info);

      // Skip the test that doesn't match the filter string (if given).
      if ((!positive_test_filter_.empty() &&
           !MatchesFilter(filtering_test_name, positive_test_filter_)) ||
          MatchesFilter(filtering_test_name, negative_test_filter_)) {
        continue;
      }

      if (!launcher_delegate_->ShouldRunTest(test_case, test_info))
        continue;

      if (num_runnable_tests++ % total_shards_ != shard_index_)
        continue;

      test_names.push_back(test_name);
    }
  }

  test_started_count_ = launcher_delegate_->RunTests(this, test_names);

  if (test_started_count_ == 0) {
    fprintf(stdout, "0 tests run\n");
    fflush(stdout);

    // No tests have actually been started, so kick off the next iteration.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        Bind(&TestLauncher::RunTestIteration, Unretained(this)));
  }
}

void TestLauncher::RunTestIteration() {
  if (cycles_ == 0) {
    MessageLoop::current()->Quit();
    return;
  }

  // Special value "-1" means "repeat indefinitely".
  cycles_ = (cycles_ == -1) ? cycles_ : cycles_ - 1;

  test_started_count_ = 0;
  test_finished_count_ = 0;
  test_success_count_ = 0;
  test_broken_count_ = 0;
  retry_count_ = 0;
  tests_to_retry_.clear();
  results_tracker_.OnTestIterationStarting();
  launcher_delegate_->OnTestIterationStarting();

  MessageLoop::current()->PostTask(
      FROM_HERE, Bind(&TestLauncher::RunTests, Unretained(this)));
}

void TestLauncher::MaybeSaveSummaryAsJSON() {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kTestLauncherSummaryOutput)) {
    FilePath summary_path(command_line->GetSwitchValuePath(
                              switches::kTestLauncherSummaryOutput));
    if (!results_tracker_.SaveSummaryAsJSON(summary_path)) {
      LOG(ERROR) << "Failed to save test launcher output summary.";
    }
  }
}

void TestLauncher::OnLaunchTestProcessFinished(
    const LaunchChildGTestProcessCallback& callback,
    int exit_code,
    const TimeDelta& elapsed_time,
    bool was_timeout,
    const std::string& output) {
  DCHECK(thread_checker_.CalledOnValidThread());

  callback.Run(exit_code, elapsed_time, was_timeout, output);
}

void TestLauncher::OnOutputTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  AutoLock lock(g_live_processes_lock.Get());

  fprintf(stdout, "Still waiting for the following processes to finish:\n");

  for (std::map<ProcessHandle, CommandLine>::iterator i =
           g_live_processes.Get().begin();
       i != g_live_processes.Get().end();
       ++i) {
#if defined(OS_WIN)
    fwprintf(stdout, L"\t%s\n", i->second.GetCommandLineString().c_str());
#else
    fprintf(stdout, "\t%s\n", i->second.GetCommandLineString().c_str());
#endif
  }

  fflush(stdout);

  // Arm the timer again - otherwise it would fire only once.
  watchdog_timer_.Reset();
}

std::string GetTestOutputSnippet(const TestResult& result,
                                 const std::string& full_output) {
  size_t run_pos = full_output.find(std::string("[ RUN      ] ") +
                                    result.full_name);
  if (run_pos == std::string::npos)
    return std::string();

  size_t end_pos = full_output.find(std::string("[  FAILED  ] ") +
                                    result.full_name,
                                    run_pos);
  // Only clip the snippet to the "OK" message if the test really
  // succeeded. It still might have e.g. crashed after printing it.
  if (end_pos == std::string::npos &&
      result.status == TestResult::TEST_SUCCESS) {
    end_pos = full_output.find(std::string("[       OK ] ") +
                               result.full_name,
                               run_pos);
  }
  if (end_pos != std::string::npos) {
    size_t newline_pos = full_output.find("\n", end_pos);
    if (newline_pos != std::string::npos)
      end_pos = newline_pos + 1;
  }

  std::string snippet(full_output.substr(run_pos));
  if (end_pos != std::string::npos)
    snippet = full_output.substr(run_pos, end_pos - run_pos);

  return snippet;
}

int LaunchChildGTestProcess(const CommandLine& command_line,
                            const std::string& wrapper,
                            base::TimeDelta timeout,
                            bool* was_timeout) {
  LaunchOptions options;

#if defined(OS_POSIX)
  // On POSIX, we launch the test in a new process group with pgid equal to
  // its pid. Any child processes that the test may create will inherit the
  // same pgid. This way, if the test is abruptly terminated, we can clean up
  // any orphaned child processes it may have left behind.
  options.new_process_group = true;
#endif

  return LaunchChildTestProcessWithOptions(
      PrepareCommandLineForGTest(command_line, wrapper),
      options,
      timeout,
      was_timeout);
}

CommandLine PrepareCommandLineForGTest(const CommandLine& command_line,
                                       const std::string& wrapper) {
  CommandLine new_command_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Strip out gtest_repeat flag - this is handled by the launcher process.
  switches.erase(kGTestRepeatFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Prepend wrapper after last CommandLine quasi-copy operation. CommandLine
  // does not really support removing switches well, and trying to do that
  // on a CommandLine with a wrapper is known to break.
  // TODO(phajdan.jr): Give it a try to support CommandLine removing switches.
#if defined(OS_WIN)
  new_command_line.PrependWrapper(ASCIIToWide(wrapper));
#elif defined(OS_POSIX)
  new_command_line.PrependWrapper(wrapper);
#endif

  return new_command_line;
}

int LaunchChildTestProcessWithOptions(const CommandLine& command_line,
                                      const LaunchOptions& options,
                                      base::TimeDelta timeout,
                                      bool* was_timeout) {
#if defined(OS_POSIX)
  // Make sure an option we rely on is present - see LaunchChildGTestProcess.
  DCHECK(options.new_process_group);
#endif

  LaunchOptions new_options(options);

#if defined(OS_WIN)
  DCHECK(!new_options.job_handle);

  win::ScopedHandle job_handle(CreateJobObject(NULL, NULL));
  if (!job_handle.IsValid()) {
    LOG(ERROR) << "Could not create JobObject.";
    return -1;
  }

  // Allow break-away from job since sandbox and few other places rely on it
  // on Windows versions prior to Windows 8 (which supports nested jobs).
  // TODO(phajdan.jr): Do not allow break-away on Windows 8.
  if (!SetJobObjectLimitFlags(job_handle.Get(),
                              JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                              JOB_OBJECT_LIMIT_BREAKAWAY_OK)) {
    LOG(ERROR) << "Could not SetJobObjectLimitFlags.";
    return -1;
  }

  new_options.job_handle = job_handle.Get();
#endif  // defined(OS_WIN)

  base::ProcessHandle process_handle;

  {
    // Note how we grab the lock before the process possibly gets created.
    // This ensures that when the lock is held, ALL the processes are registered
    // in the set.
    AutoLock lock(g_live_processes_lock.Get());

    if (!base::LaunchProcess(command_line, new_options, &process_handle))
      return -1;

    g_live_processes.Get().insert(std::make_pair(process_handle, command_line));
  }

  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle,
                                        &exit_code,
                                        timeout)) {
    *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    // Ensure that the process terminates.
    base::KillProcess(process_handle, -1, true);
  }

  {
    // Note how we grab the log before issuing a possibly broad process kill.
    // Other code parts that grab the log kill processes, so avoid trying
    // to do that twice and trigger all kinds of log messages.
    AutoLock lock(g_live_processes_lock.Get());

#if defined(OS_POSIX)
    if (exit_code != 0) {
      // On POSIX, in case the test does not exit cleanly, either due to a crash
      // or due to it timing out, we need to clean up any child processes that
      // it might have created. On Windows, child processes are automatically
      // cleaned up using JobObjects.
      base::KillProcessGroup(process_handle);
    }
#endif

    g_live_processes.Get().erase(process_handle);
  }

  base::CloseProcessHandle(process_handle);

  return exit_code;
}

}  // namespace base
