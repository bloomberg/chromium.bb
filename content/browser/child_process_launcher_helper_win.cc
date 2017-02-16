// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/common/sandbox_win.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "mojo/edk/embedder/named_platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "sandbox/win/src/sandbox_types.h"

namespace content {
namespace internal {

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
}

mojo::edk::ScopedPlatformHandle
ChildProcessLauncherHelper::PrepareMojoPipeHandlesOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);

  if (!delegate_->ShouldLaunchElevated())
    return mojo::edk::ScopedPlatformHandle();

  mojo::edk::NamedPlatformChannelPair named_pair;
  named_pair.PrepareToPassClientHandleToChildProcess(command_line());
  return named_pair.PassServerHandle();
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  return std::unique_ptr<FileMappedForLaunch>();
}

void ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const FileMappedForLaunch& files_to_register,
    base::LaunchOptions* options) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  *is_synchronous_launch = true;
  if (delegate_->ShouldLaunchElevated()) {
    // When establishing a Mojo connection, the pipe path has already been added
    // to the command line.
    base::LaunchOptions win_options;
    win_options.start_hidden = true;
    ChildProcessLauncherHelper::Process process;
    process.process = base::LaunchElevatedProcess(*command_line(), win_options);
    return process;
  }
  base::HandlesToInheritVector handles;
  handles.push_back(mojo_client_handle().handle);
  base::FieldTrialList::AppendFieldTrialHandleIfNeeded(&handles);
  command_line()->AppendSwitchASCII(
      mojo::edk::PlatformChannelPair::kMojoPlatformChannelHandleSwitch,
      base::UintToString(base::win::HandleToUint32(handles[0])));
  ChildProcessLauncherHelper::Process process;
  *launch_result = StartSandboxedProcess(
      delegate_.get(),
      command_line(),
      handles,
      &process.process);
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
}

// static
base::TerminationStatus ChildProcessLauncherHelper::GetTerminationStatus(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead,
    int* exit_code) {
  return base::GetTerminationStatus(process.process.Handle(), exit_code);
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(
    const base::Process& process, int exit_code, bool wait) {
  return process.Terminate(exit_code, wait);
}

void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  // Client has gone away, so just kill the process.  Using exit code 0 means
  // that UMA won't treat this as a crash.
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
}

// static
void ChildProcessLauncherHelper::SetProcessBackgroundedOnLauncherThread(
      base::Process process, bool background) {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  if (process.CanBackgroundProcesses())
    process.SetProcessBackgrounded(background);
}

// static
void ChildProcessLauncherHelper::SetRegisteredFilesForService(
    const std::string& service_name,
    catalog::RequiredFileMap required_files) {
  // No file passing from the manifest on Windows yet.
  DCHECK(required_files.empty());
}

}  // namespace internal
}  // namespace content
