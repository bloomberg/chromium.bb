// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/child_process_launcher.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "build/build_config.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"

namespace content {

using internal::ChildProcessLauncherHelper;

ChildProcessLauncher::ChildProcessLauncher(
    std::unique_ptr<SandboxedProcessLauncherDelegate> delegate,
    std::unique_ptr<base::CommandLine> command_line,
    int child_process_id,
    Client* client,
    std::unique_ptr<mojo::edk::OutgoingBrokerClientInvitation>
        broker_client_invitation,
    const mojo::edk::ProcessErrorCallback& process_error_callback,
    bool terminate_on_shutdown)
    : client_(client),
      termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
      exit_code_(RESULT_CODE_NORMAL_EXIT),
      starting_(true),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
      terminate_child_on_shutdown_(false),
#else
      terminate_child_on_shutdown_(terminate_on_shutdown),
#endif
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));

  helper_ = new ChildProcessLauncherHelper(
      child_process_id, client_thread_id_, std::move(command_line),
      std::move(delegate), weak_factory_.GetWeakPtr(), terminate_on_shutdown,
      std::move(broker_client_invitation), process_error_callback);
  helper_->StartLaunchOnClientThread();
}

ChildProcessLauncher::~ChildProcessLauncher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_.process.IsValid() && terminate_child_on_shutdown_) {
    // Client has gone away, so just kill the process.
    ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
        std::move(process_));
  }
}

void ChildProcessLauncher::SetProcessPriority(
    const ChildProcessLauncherPriority& priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Process to_pass = process_.process.Duplicate();
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::BindOnce(
          &ChildProcessLauncherHelper::SetProcessPriorityOnLauncherThread,
          helper_, base::Passed(&to_pass), priority));
}

void ChildProcessLauncher::Notify(
    ChildProcessLauncherHelper::Process process,
    int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  starting_ = false;
  process_ = std::move(process);

  if (process_.process.IsValid()) {
    client_->OnProcessLaunched();
  } else {
    termination_status_ = base::TERMINATION_STATUS_LAUNCH_FAILED;

    // NOTE: May delete |this|.
    client_->OnProcessLaunchFailed(error_code);
  }
}

bool ChildProcessLauncher::IsStarting() {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return process_.process;
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    bool known_dead,
    int* exit_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!process_.process.IsValid()) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = exit_code_;
    return termination_status_;
  }

  termination_status_ =
      helper_->GetTerminationStatus(process_, known_dead, &exit_code_);
  if (exit_code)
    *exit_code = exit_code_;

  // POSIX: If the process crashed, then the kernel closed the socket for it and
  // so the child has already died by the time we get here. Since
  // GetTerminationStatus called waitpid with WNOHANG, it'll reap the process.
  // However, if GetTerminationStatus didn't reap the child (because it was
  // still running), we'll need to Terminate via ProcessWatcher. So we can't
  // close the handle here.
  if (termination_status_ != base::TERMINATION_STATUS_STILL_RUNNING) {
    process_.process.Exited(exit_code_);
    process_.process.Close();
  }

  return termination_status_;
}

bool ChildProcessLauncher::Terminate(int exit_code, bool wait) {
  return IsStarting() ? false
                      : ChildProcessLauncherHelper::TerminateProcess(
                            GetProcess(), exit_code, wait);
}

// static
bool ChildProcessLauncher::TerminateProcess(const base::Process& process,
                                            int exit_code,
                                            bool wait) {
  return ChildProcessLauncherHelper::TerminateProcess(process, exit_code, wait);
}

// static
void ChildProcessLauncher::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
  ChildProcessLauncherHelper::SetRegisteredFilesForService(
      service_name, std::move(required_files));
}

// static
void ChildProcessLauncher::ResetRegisteredFilesForTesting() {
  ChildProcessLauncherHelper::ResetRegisteredFilesForTesting();
}

#if defined(OS_ANDROID)
// static
size_t ChildProcessLauncher::GetNumberOfRendererSlots() {
  return ChildProcessLauncherHelper::GetNumberOfRendererSlots();
}
#endif  // OS_ANDROID

ChildProcessLauncher::Client* ChildProcessLauncher::ReplaceClientForTest(
    Client* client) {
  Client* ret = client_;
  client_ = client;
  return ret;
}

bool ChildProcessLauncherPriority::operator==(
    const ChildProcessLauncherPriority& other) const {
  return background == other.background &&
         boost_for_pending_views == other.boost_for_pending_views
#if defined(OS_ANDROID)
         && importance == other.importance
#endif
      ;
}

}  // namespace content
