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
#include "base/task_runner.h"
#include "base/task_runner_util.h"
#include "mojo/shell/context.h"
#include "mojo/shell/switches.h"

namespace mojo {
namespace shell {

ChildProcessHost::ChildProcessHost(Context* context) : context_(context) {
  platform_channel_ = platform_channel_pair_.PassServerHandle();
  CHECK(platform_channel_.is_valid());
}

ChildProcessHost::~ChildProcessHost() {
  if (child_process_.IsValid()) {
    LOG(WARNING) << "Destroying ChildProcessHost with unjoined child";
    child_process_.Close();
  }
}

void ChildProcessHost::Start() {
  DCHECK(!child_process_.IsValid());

  WillStart();

  CHECK(base::PostTaskAndReplyWithResult(
      context_->task_runners()->blocking_pool(), FROM_HERE,
      base::Bind(&ChildProcessHost::DoLaunch, base::Unretained(this)),
      base::Bind(&ChildProcessHost::DidStart, base::Unretained(this))));
}

int ChildProcessHost::Join() {
  DCHECK(child_process_.IsValid());
  int rv = -1;
  LOG_IF(ERROR, !child_process_.WaitForExit(&rv))
      << "Failed to wait for child process";
  child_process_.Close();
  return rv;
}

bool ChildProcessHost::DoLaunch() {
  static const char* kForwardSwitches[] = {
      switches::kTraceToConsole, switches::kV, switches::kVModule,
  };

  const base::CommandLine* parent_command_line =
      base::CommandLine::ForCurrentProcess();
  base::CommandLine child_command_line(parent_command_line->GetProgram());
  child_command_line.CopySwitchesFrom(*parent_command_line, kForwardSwitches,
                                      arraysize(kForwardSwitches));
  child_command_line.AppendSwitch(switches::kChildProcess);

  embedder::HandlePassingInformation handle_passing_info;
  platform_channel_pair_.PrepareToPassClientHandleToChildProcess(
      &child_command_line, &handle_passing_info);

  base::LaunchOptions options;
#if defined(OS_WIN)
  options.start_hidden = true;
  options.handles_to_inherit = &handle_passing_info;
#elif defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#endif
  DVLOG(2) << "Launching child with command line: "
           << child_command_line.GetCommandLineString();
  child_process_ = base::LaunchProcess(child_command_line, options);
  if (!child_process_.IsValid())
    return false;

  platform_channel_pair_.ChildProcessLaunched();
  return true;
}

}  // namespace shell
}  // namespace mojo
