// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/child_process_host.h"

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"
#include "mojo/system/embedder/platform_channel_pair.h"

namespace mojo {
namespace shell {

ChildProcessHost::ChildProcessHost(Context* context, ChildProcess::Type type)
    : process_launch_task_runner_(context->task_runners()->blocking_pool()),
      type_(type),
      child_process_handle_(base::kNullProcessHandle) {
}

ChildProcessHost::~ChildProcessHost() {
  if (child_process_handle_ != base::kNullProcessHandle) {
    LOG(WARNING) << "Destroying ChildProcessHost with unjoined child";
    base::CloseProcessHandle(child_process_handle_);
    child_process_handle_ = base::kNullProcessHandle;
  }
}

void ChildProcessHost::Start(DidStartCallback callback) {
  DCHECK_EQ(child_process_handle_, base::kNullProcessHandle);
  CHECK(base::PostTaskAndReplyWithResult(
      process_launch_task_runner_,
      FROM_HERE,
      base::Bind(&ChildProcessHost::DoLaunch, base::Unretained(this)),
      callback));
}

int ChildProcessHost::Join() {
  DCHECK_NE(child_process_handle_, base::kNullProcessHandle);
  int rv = -1;
  LOG_IF(ERROR, !base::WaitForExitCode(child_process_handle_, &rv))
      << "Failed to wait for child process";
  base::CloseProcessHandle(child_process_handle_);
  child_process_handle_ = base::kNullProcessHandle;
  return rv;
}

bool ChildProcessHost::DoLaunch() {
  static const char* kForwardSwitches[] = {
    switches::kTraceToConsole,
    switches::kV,
    switches::kVModule,
  };

  const CommandLine* parent_command_line = CommandLine::ForCurrentProcess();
  CommandLine child_command_line(parent_command_line->GetProgram());
  child_command_line.CopySwitchesFrom(*parent_command_line, kForwardSwitches,
                                      arraysize(kForwardSwitches));
  child_command_line.AppendSwitchASCII(
      switches::kChildProcessType, base::IntToString(static_cast<int>(type_)));

  embedder::PlatformChannelPair platform_channel_pair;
  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair.PrepareToPassClientHandleToChildProcess(
      &child_command_line, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#endif

  if (!base::LaunchProcess(child_command_line, options, &child_process_handle_))
    return false;

  platform_channel_pair.ChildProcessLaunched();
  platform_channel_ = platform_channel_pair.PassServerHandle();
  CHECK(platform_channel_.is_valid());
  return true;
}

}  // namespace shell
}  // namespace mojo
