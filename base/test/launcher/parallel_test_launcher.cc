// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/parallel_test_launcher.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/process/launch.h"
#include "base/run_loop.h"
#include "base/test/launcher/test_launcher.h"
#include "base/test/sequenced_worker_pool_owner.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {

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

void RunCallback(
    const ParallelTestLauncher::LaunchChildGTestProcessCallback& callback,
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
    const ParallelTestLauncher::LaunchChildGTestProcessCallback& callback) {
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

ParallelTestLauncher::ParallelTestLauncher(size_t jobs)
    : timer_(FROM_HERE,
             TimeDelta::FromSeconds(kOutputTimeoutSeconds),
             this,
             &ParallelTestLauncher::OnOutputTimeout),
      launch_sequence_number_(0),
      worker_pool_owner_(
          new SequencedWorkerPoolOwner(jobs, "parallel_test_launcher")) {
  // Start the watchdog timer.
  timer_.Reset();
}

ParallelTestLauncher::~ParallelTestLauncher() {
  DCHECK(thread_checker_.CalledOnValidThread());

  worker_pool_owner_->pool()->Shutdown();
}

void ParallelTestLauncher::LaunchChildGTestProcess(
    const CommandLine& command_line,
    const std::string& wrapper,
    base::TimeDelta timeout,
    const LaunchChildGTestProcessCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Record the exact command line used to launch the child.
  CommandLine new_command_line(
      PrepareCommandLineForGTest(command_line, wrapper));
  launch_sequence_number_++;
  running_processes_map_.insert(
      std::make_pair(launch_sequence_number_, new_command_line));

  worker_pool_owner_->pool()->PostWorkerTask(
      FROM_HERE,
      Bind(&DoLaunchChildTestProcess,
           new_command_line,
           timeout,
           MessageLoopProxy::current(),
           Bind(&ParallelTestLauncher::OnLaunchTestProcessFinished,
                Unretained(this),
                launch_sequence_number_,
                callback)));
}

void ParallelTestLauncher::ResetOutputWatchdog() {
  DCHECK(thread_checker_.CalledOnValidThread());
  timer_.Reset();
}

void ParallelTestLauncher::OnLaunchTestProcessFinished(
    size_t sequence_number,
    const LaunchChildGTestProcessCallback& callback,
    int exit_code,
    const TimeDelta& elapsed_time,
    bool was_timeout,
    const std::string& output) {
  DCHECK(thread_checker_.CalledOnValidThread());
  running_processes_map_.erase(sequence_number);
  callback.Run(exit_code, elapsed_time, was_timeout, output);
}

void ParallelTestLauncher::OnOutputTimeout() {
  DCHECK(thread_checker_.CalledOnValidThread());

  fprintf(stdout, "Still waiting for the following processes to finish:\n");

  for (RunningProcessesMap::const_iterator i = running_processes_map_.begin();
       i != running_processes_map_.end();
       ++i) {
#if defined(OS_WIN)
    fwprintf(stdout, L"\t%s\n", i->second.GetCommandLineString().c_str());
#else
    fprintf(stdout, "\t%s\n", i->second.GetCommandLineString().c_str());
#endif
  }

  fflush(stdout);

  // Arm the timer again - otherwise it would fire only once.
  timer_.Reset();
}

}  // namespace base
