// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/conflicts/module_inspector_win.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/conflicts/module_info_util_win.h"
#include "chrome/services/util_win/public/mojom/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {

constexpr base::Feature kWinOOPInspectModuleFeature = {
    "WinOOPInspectModule", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr int kConnectionErrorRetryCount = 10;

StringMapping GetPathMapping() {
  return GetEnvironmentVariablesMapping({
      L"LOCALAPPDATA", L"ProgramFiles", L"ProgramData", L"USERPROFILE",
      L"SystemRoot", L"TEMP", L"TMP", L"CommonProgramFiles",
  });
}

// Returns a bound pointer to the UtilWin service.
chrome::mojom::UtilWinPtr ConnectToUtilWinService(
    base::OnceClosure connection_error_handler) {
  chrome::mojom::UtilWinPtr util_win_ptr;

  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(chrome::mojom::kUtilWinServiceName, &util_win_ptr);

  util_win_ptr.set_connection_error_handler(
      std::move(connection_error_handler));

  return util_win_ptr;
}

void ReportConnectionError(bool value) {
  base::UmaHistogramBoolean("Windows.InspectModule.ConnectionError", value);
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
      connection_error_retry_count_(kConnectionErrorRetryCount),
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

void ModuleInspector::OnUtilWinServiceConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReportConnectionError(true);

  // Reset the pointer to the service.
  util_win_ptr_ = nullptr;

  // Restart inspection for the current module, only if the retry limit wasn't
  // reached.
  if (connection_error_retry_count_--)
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
  if (base::FeatureList::IsEnabled(kWinOOPInspectModuleFeature)) {
    // Make sure the pointer is bound to the service first.
    if (!util_win_ptr_) {
      util_win_ptr_ = ConnectToUtilWinService(
          base::BindOnce(&ModuleInspector::OnUtilWinServiceConnectionError,
                         base::Unretained(this)));
      ReportConnectionError(false);
    }

    util_win_ptr_->InspectModule(
        module_key.module_path,
        base::BindOnce(&ModuleInspector::OnInspectionFinished,
                       weak_ptr_factory_.GetWeakPtr(), module_key));
  } else {
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&InspectModule, module_key.module_path),
        base::BindOnce(&ModuleInspector::OnInspectionFinished,
                       weak_ptr_factory_.GetWeakPtr(), module_key));
  }
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

  // Free the pointer to the UtilWin service to clean up the utility process
  // when it is no longer needed. While this code is only ever needed in the
  // case the WinOOPInspectModule feature is enabled, it's faster to check the
  // value of the pointer than to check the feature status.
  if (queue_.empty() && util_win_ptr_)
    util_win_ptr_ = nullptr;

  // Continue the work.
  if (!queue_.empty())
    StartInspectingModule();
}
