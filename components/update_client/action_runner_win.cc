// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <utility>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"

namespace {

const base::FilePath::CharType kRecoveryFileName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

}  // namespace

namespace update_client {

void ActionRunner::RunCommand(const base::CommandLine& cmdline) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(cmdline, options);

  // This task joins a process, hence .WithBaseSyncPrimitives().
  base::PostTaskWithTraits(
      FROM_HERE,
      {base::WithBaseSyncPrimitives(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ActionRunner::WaitForCommand, base::Unretained(this),
                     std::move(process)));
}

void ActionRunner::WaitForCommand(base::Process process) {
  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);

  main_task_runner_->PostTask(
      FROM_HERE, base::Bind(run_complete_, succeeded, exit_code, 0));
}

base::CommandLine ActionRunner::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  return base::CommandLine(unpack_path.Append(kRecoveryFileName));
}

}  // namespace update_client
