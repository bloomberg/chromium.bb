// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/posix/global_descriptors.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/child_process_launcher_helper.h"
#include "content/browser/child_process_launcher_helper_posix.h"
#include "content/browser/renderer_host/render_sandbox_host_linux.h"
#include "content/browser/zygote_host/zygote_communication_linux.h"
#include "content/browser/zygote_host/zygote_host_impl_linux.h"
#include "content/common/sandbox_linux/sandbox_linux.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/zygote_handle_linux.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/sandboxed_process_launcher_delegate.h"
#include "gin/v8_initializer.h"

namespace content {
namespace internal {

mojo::edk::ScopedPlatformHandle
ChildProcessLauncherHelper::PrepareMojoPipeHandlesOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
  return mojo::edk::ScopedPlatformHandle();
}

void ChildProcessLauncherHelper::BeforeLaunchOnClientThread() {
  DCHECK_CURRENTLY_ON(client_thread_id_);
}

std::unique_ptr<FileMappedForLaunch>
ChildProcessLauncherHelper::GetFilesToMap() {
  DCHECK_CURRENTLY_ON(BrowserThread::PROCESS_LAUNCHER);
  return CreateDefaultPosixFilesToMap(child_process_id(), mojo_client_handle(),
                                      true /* include_service_required_files */,
                                      GetProcessType(), command_line());
}

void ChildProcessLauncherHelper::BeforeLaunchOnLauncherThread(
    const FileDescriptorInfo& files_to_register,
    base::LaunchOptions* options) {
  // Convert FD mapping to FileHandleMappingVector
  std::unique_ptr<base::FileHandleMappingVector> fds_to_map =
      files_to_register.GetMappingWithIDAdjustment(
          base::GlobalDescriptors::kBaseDescriptor);

  if (GetProcessType() == switches::kRendererProcess) {
    const int sandbox_fd =
        RenderSandboxHostLinux::GetInstance()->GetRendererSocket();
    fds_to_map->push_back(std::make_pair(sandbox_fd, GetSandboxFD()));
  }

  options->environ = delegate_->GetEnvironment();
  // fds_to_remap will de deleted in AfterLaunchOnLauncherThread() below.
  options->fds_to_remap = fds_to_map.release();
}

ChildProcessLauncherHelper::Process
ChildProcessLauncherHelper::LaunchProcessOnLauncherThread(
    const base::LaunchOptions& options,
    std::unique_ptr<FileMappedForLaunch> files_to_register,
    bool* is_synchronous_launch,
    int* launch_result) {
  *is_synchronous_launch = true;

  ZygoteHandle* zygote_handle =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoZygote) ?
      nullptr : delegate_->GetZygote();
  if (zygote_handle) {
    // This code runs on the PROCESS_LAUNCHER thread so race conditions are not
    // an issue with the lazy initialization.
    if (*zygote_handle == nullptr) {
      *zygote_handle = CreateZygote();
    }
    base::ProcessHandle handle = (*zygote_handle)->ForkRequest(
        command_line()->argv(),
        std::move(files_to_register),
        GetProcessType());
    *launch_result = LAUNCH_RESULT_SUCCESS;
    Process process;
    process.process = base::Process(handle);
    process.zygote = *zygote_handle;
    return process;
  }

  Process process;
  process.process = base::LaunchProcess(*command_line(), options);
  *launch_result = process.process.IsValid() ? LAUNCH_RESULT_SUCCESS
                                             : LAUNCH_RESULT_FAILURE;
  return process;
}

void ChildProcessLauncherHelper::AfterLaunchOnLauncherThread(
    const ChildProcessLauncherHelper::Process& process,
    const base::LaunchOptions& options) {
  delete options.fds_to_remap;
}

// static
base::TerminationStatus ChildProcessLauncherHelper::GetTerminationStatus(
    const ChildProcessLauncherHelper::Process& process,
    bool known_dead,
    int* exit_code) {
  if (process.zygote) {
    return process.zygote->GetTerminationStatus(
        process.process.Handle(), known_dead, exit_code);
  }
  if (known_dead) {
    return base::GetKnownDeadTerminationStatus(
        process.process.Handle(), exit_code);
  }
  return base::GetTerminationStatus(process.process.Handle(), exit_code);
}

// static
bool ChildProcessLauncherHelper::TerminateProcess(
    const base::Process& process, int exit_code, bool wait) {
  return process.Terminate(exit_code, wait);
}

// static
void ChildProcessLauncherHelper::ForceNormalProcessTerminationSync(
    ChildProcessLauncherHelper::Process process) {
  process.process.Terminate(RESULT_CODE_NORMAL_EXIT, false);
  // On POSIX, we must additionally reap the child.
  if (process.zygote) {
    // If the renderer was created via a zygote, we have to proxy the reaping
    // through the zygote process.
    process.zygote->EnsureProcessTerminated(process.process.Handle());
  } else {
    base::EnsureProcessTerminated(std::move(process.process));
  }
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
  SetFilesToShareForServicePosix(service_name, std::move(required_files));
}

// static
base::File OpenFileToShare(const base::FilePath& path,
                           base::MemoryMappedFile::Region* region) {
  base::FilePath exe_dir;
  bool result = base::PathService::Get(base::BasePathKey::DIR_EXE, &exe_dir);
  DCHECK(result);
  base::File file(exe_dir.Append(path),
                  base::File::FLAG_OPEN | base::File::FLAG_READ);
  *region = base::MemoryMappedFile::Region::kWholeFile;
  return file;
}

}  // namespace internal
}  // namespace content
