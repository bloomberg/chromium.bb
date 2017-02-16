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
    std::unique_ptr<mojo::edk::PendingProcessConnection> pending_connection,
    const mojo::edk::ProcessErrorCallback& process_error_callback,
    bool terminate_on_shutdown)
    : client_(client),
      termination_status_(base::TERMINATION_STATUS_NORMAL_TERMINATION),
      exit_code_(RESULT_CODE_NORMAL_EXIT),
      starting_(true),
      pending_connection_(std::move(pending_connection)),
      process_error_callback_(process_error_callback),
#if defined(ADDRESS_SANITIZER) || defined(LEAK_SANITIZER) ||  \
    defined(MEMORY_SANITIZER) || defined(THREAD_SANITIZER) || \
    defined(UNDEFINED_SANITIZER)
      terminate_child_on_shutdown_(false),
#else
      terminate_child_on_shutdown_(terminate_on_shutdown),
#endif
      weak_factory_(this) {
  DCHECK(CalledOnValidThread());
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&client_thread_id_));

  helper_ = new ChildProcessLauncherHelper(
                child_process_id, client_thread_id_,
              std::move(command_line), std::move(delegate),
                       weak_factory_.GetWeakPtr(), terminate_on_shutdown);
  helper_->StartLaunchOnClientThread();
}

ChildProcessLauncher::~ChildProcessLauncher() {
  DCHECK(CalledOnValidThread());
  if (process_.process.IsValid() && terminate_child_on_shutdown_) {
    // Client has gone away, so just kill the process.
    ChildProcessLauncherHelper::ForceNormalProcessTerminationAsync(
        std::move(process_));
  }
}

void ChildProcessLauncher::SetProcessBackgrounded(bool background) {
  DCHECK(CalledOnValidThread());
  base::Process to_pass = process_.process.Duplicate();
  BrowserThread::PostTask(
      BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
      base::Bind(
          &ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread,
          base::Passed(&to_pass),
          background));
}

void ChildProcessLauncher::Notify(
    ChildProcessLauncherHelper::Process process,
    mojo::edk::ScopedPlatformHandle server_handle,
    int error_code) {
  DCHECK(CalledOnValidThread());
  starting_ = false;
  process_ = std::move(process);

  // Take ownership of the pending connection here so it's destroyed when
  // we go out of scope regardless of the outcome below.
  std::unique_ptr<mojo::edk::PendingProcessConnection> pending_connection =
      std::move(pending_connection_);
  if (process_.process.IsValid()) {
    // Set up Mojo IPC to the new process.
    DCHECK(pending_connection);
    pending_connection->Connect(process_.process.Handle(),
                                std::move(server_handle),
                                process_error_callback_);
    client_->OnProcessLaunched();
  } else {
    termination_status_ = base::TERMINATION_STATUS_LAUNCH_FAILED;

    // NOTE: May delete |this|.
    client_->OnProcessLaunchFailed(error_code);
  }
}

bool ChildProcessLauncher::IsStarting() {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK(CalledOnValidThread());
  return starting_;
}

const base::Process& ChildProcessLauncher::GetProcess() const {
  // TODO(crbug.com/469248): This fails in some tests.
  // DCHECK(CalledOnValidThread());
  return process_.process;
}

base::TerminationStatus ChildProcessLauncher::GetChildTerminationStatus(
    bool known_dead,
    int* exit_code) {
  DCHECK(CalledOnValidThread());
  if (!process_.process.IsValid()) {
    // Process is already gone, so return the cached termination status.
    if (exit_code)
      *exit_code = exit_code_;
    return termination_status_;
  }

  termination_status_ = ChildProcessLauncherHelper::GetTerminationStatus(
      process_, known_dead, &exit_code_);
  if (exit_code)
    *exit_code = exit_code_;

  // POSIX: If the process crashed, then the kernel closed the socket for it and
  // so the child has already died by the time we get here. Since
  // GetTerminationStatus called waitpid with WNOHANG, it'll reap the process.
  // However, if GetTerminationStatus didn't reap the child (because it was
  // still running), we'll need to Terminate via ProcessWatcher. So we can't
  // close the handle here.
  if (termination_status_ != base::TERMINATION_STATUS_STILL_RUNNING)
    process_.process.Close();

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

ChildProcessLauncher::Client* ChildProcessLauncher::ReplaceClientForTest(
    Client* client) {
  Client* ret = client_;
  client_ = client;
  return ret;
}

}  // namespace content
