// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_inspector_win.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/conflicts/module_info_util_win.h"

namespace {

StringMapping GetPathMapping() {
  return GetEnvironmentVariablesMapping({
      L"LOCALAPPDATA", L"ProgramFiles", L"ProgramData", L"USERPROFILE",
      L"SystemRoot", L"TEMP", L"TMP", L"CommonProgramFiles",
  });
}

}  // namespace

ModuleInspector::ModuleInspector(
    const OnModuleInspectedCallback& on_module_inspected_callback)
    : on_module_inspected_callback_(on_module_inspected_callback),
      is_after_startup_(false),
      task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      path_mapping_(GetPathMapping()),
      weak_ptr_factory_(this) {
  // Use AfterStartupTaskUtils to be notified when startup is finished.
  AfterStartupTaskUtils::PostTask(
      FROM_HERE, base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(&ModuleInspector::OnStartupFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

ModuleInspector::~ModuleInspector() = default;

void ModuleInspector::AddModule(const ModuleInfoKey& module_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool was_queue_empty = queue_.empty();

  queue_.push(module_key);

  // If the queue was empty before adding the current module, then the
  // inspection must be started.
  if (is_after_startup_ && was_queue_empty)
    StartInspectingModule();
}

void ModuleInspector::IncreaseInspectionPriority() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Create a task runner with higher priority so that future inspections are
  // done faster.
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // Assume startup is finished to immediately begin inspecting modules.
  OnStartupFinished();
}

bool ModuleInspector::IsIdle() {
  return queue_.empty();
}

void ModuleInspector::OnStartupFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This function will be invoked twice if IncreaseInspectionPriority() is
  // called.
  if (is_after_startup_)
    return;

  is_after_startup_ = true;

  if (!queue_.empty())
    StartInspectingModule();
}

void ModuleInspector::StartInspectingModule() {
  DCHECK(is_after_startup_);
  DCHECK(!queue_.empty());

  const ModuleInfoKey& module_key = queue_.front();

  // There is a small priority inversion that happens when
  // IncreaseInspectionPriority() is called while a module is currently being
  // inspected.
  //
  // This is because all the subsequent tasks will be posted at a higher
  // priority, but they are waiting on the current task that is currently
  // running at a lower priority.
  //
  // In practice, this is not an issue because the only caller of
  // IncreaseInspectionPriority() (chrome://conflicts) does not depend on the
  // inspection to finish synchronously and is not blocking anything else.
  base::PostTaskAndReplyWithResult(
      task_runner_.get(), FROM_HERE,
      base::BindOnce(&InspectModule, module_key.module_path),
      base::BindOnce(&ModuleInspector::OnInspectionFinished,
                     weak_ptr_factory_.GetWeakPtr(), module_key));
}

void ModuleInspector::OnInspectionFinished(
    const ModuleInfoKey& module_key,
    ModuleInspectionResult inspection_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Convert the prefix of known Windows directories to their environment
  // variable mappings (ie, %systemroot$). This makes i18n localized paths
  // easily comparable.
  CollapseMatchingPrefixInPath(path_mapping_, &inspection_result.location);

  // Pop first, because the callback may want to know if there is any work left
  // to be done, which is caracterized by a non-empty queue.
  queue_.pop();

  on_module_inspected_callback_.Run(module_key, std::move(inspection_result));

  // Continue the work.
  if (!queue_.empty())
    StartInspectingModule();
}
