// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/update_client/component.h"
#include "components/update_client/update_client.h"

namespace update_client {

ActionRunner::ActionRunner(
    const Component& component,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const std::vector<uint8_t>& key_hash)
    : component_(component),
      task_runner_(task_runner),
      key_hash_(key_hash),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ActionRunner::~ActionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ActionRunner::Run(const Callback& run_complete) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  run_complete_ = run_complete;

  task_runner_->PostTask(
      FROM_HERE, base::Bind(&ActionRunner::Unpack, base::Unretained(this)));
}

void ActionRunner::Unpack() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  const auto& installer = component_.crx_component().installer;

  base::FilePath file_path;
  installer->GetInstalledFile(component_.action_run(), &file_path);

  auto unpacker = base::MakeRefCounted<ComponentUnpacker>(
      key_hash_, file_path, installer, nullptr, task_runner_);

  unpacker->Unpack(
      base::Bind(&ActionRunner::UnpackComplete, base::Unretained(this)));
}

void ActionRunner::UnpackComplete(const ComponentUnpacker::Result& result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (result.error != UnpackerError::kNone) {
    DCHECK(!base::DirectoryExists(result.unpack_path));

    main_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(run_complete_, false, static_cast<int>(result.error),
                   result.extended_error));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ActionRunner::RunCommand, base::Unretained(this),
                     MakeCommandLine(result.unpack_path)));
}

#if !defined(OS_WIN)

void ActionRunner::RunCommand(const base::CommandLine& cmdline) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(run_complete_, false, -1, 0));
}

base::CommandLine ActionRunner::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  return base::CommandLine(base::CommandLine::NO_PROGRAM);
}

#endif  // OS_WIN

}  // namespace update_client
