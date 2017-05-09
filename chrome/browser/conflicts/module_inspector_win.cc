// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_inspector_win.h"

#include <utility>

#include "base/bind.h"
#include "base/task_scheduler/post_task.h"

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
      inspection_task_priority_(base::TaskPriority::BACKGROUND),
      path_mapping_(GetPathMapping()),
      weak_ptr_factory_(this) {}

ModuleInspector::~ModuleInspector() = default;

void ModuleInspector::AddModule(const ModuleInfoKey& module_key) {
  DCHECK(thread_checker_.CalledOnValidThread());
  queue_.push(module_key);
  if (queue_.size() == 1)
    StartInspectingModule();
}

void ModuleInspector::IncreaseInspectionPriority() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Modify the TaskPriority so that future inspections are done faster.
  inspection_task_priority_ = base::TaskPriority::USER_VISIBLE;
}

void ModuleInspector::StartInspectingModule() {
  ModuleInfoKey module_key = queue_.front();
  queue_.pop();

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), inspection_task_priority_,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&InspectModule, path_mapping_, module_key),
      base::BindOnce(&ModuleInspector::OnInspectionFinished,
                     weak_ptr_factory_.GetWeakPtr(), module_key));
}

void ModuleInspector::OnInspectionFinished(
    const ModuleInfoKey& module_key,
    std::unique_ptr<ModuleInspectionResult> inspection_result) {
  on_module_inspected_callback_.Run(module_key, std::move(inspection_result));

  // Continue the work.
  if (!queue_.empty())
    StartInspectingModule();
}
