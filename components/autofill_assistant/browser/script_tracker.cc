// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"

namespace autofill_assistant {

ScriptTracker::ScriptTracker(ScriptExecutorDelegate* delegate,
                             ScriptTracker::Listener* listener)
    : delegate_(delegate),
      listener_(listener),
      running_checks_(false),
      must_recheck_(false),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetAndCheckScripts(
    std::vector<std::unique_ptr<Script>> scripts) {
  ClearAvailableScripts();
  for (auto& script : scripts) {
    available_scripts_[script.get()] = std::move(script);
  }
  CheckScripts();
}

void ScriptTracker::CheckScripts() {
  if (running_checks_) {
    must_recheck_ = true;
    return;
  }
  running_checks_ = true;

  RunPreconditionChecksSequentially(available_scripts_.begin());
}

void ScriptTracker::ExecuteScript(const std::string& script_path,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executed_scripts_[script_path] = SCRIPT_STATUS_RUNNING;
  executor_ = std::make_unique<ScriptExecutor>(script_path, delegate_);
  executor_->Run(base::BindOnce(&ScriptTracker::OnScriptRun,
                                weak_ptr_factory_.GetWeakPtr(), script_path,
                                std::move(callback)));
}

void ScriptTracker::ClearRunnableScripts() {
  runnable_scripts_.clear();
  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::OnScriptRun(
    const std::string& script_path,
    ScriptExecutor::RunScriptCallback original_callback,
    ScriptExecutor::Result result) {
  executor_.reset();
  std::move(original_callback).Run(result);
}

void ScriptTracker::UpdateRunnableScriptsIfNecessary() {
  bool runnables_changed = RunnablesHaveChanged();
  if (runnables_changed) {
    runnable_scripts_.clear();
    std::sort(pending_runnable_scripts_.begin(),
              pending_runnable_scripts_.end(),
              [](const Script* a, const Script* b) {
                // Runnable scripts with lowest priority value are displayed
                // first. The display order of scripts with the same priority is
                // arbitrary. Fallback to ordering by name, arbitrarily, for the
                // behavior to be consistent.
                return std::tie(a->priority, a->handle.name) <
                       std::tie(b->priority, b->handle.name);
              });
    for (Script* script : pending_runnable_scripts_) {
      runnable_scripts_.push_back(script->handle);
    }
  }
  pending_runnable_scripts_.clear();

  running_checks_ = false;
  if (runnables_changed) {
    listener_->OnRunnableScriptsChanged(runnable_scripts_);
  }
  if (must_recheck_) {
    must_recheck_ = false;
    CheckScripts();
  }
}

bool ScriptTracker::RunnablesHaveChanged() {
  if (runnable_scripts_.size() != pending_runnable_scripts_.size())
    return true;

  std::set<std::string> pending_paths;
  for (Script* script : pending_runnable_scripts_) {
    pending_paths.insert(script->handle.path);
  }
  std::set<std::string> current_paths;
  for (const auto& handle : runnable_scripts_) {
    current_paths.insert(handle.path);
  }
  return pending_paths != current_paths;
}

void ScriptTracker::RunPreconditionChecksSequentially(
    AvailableScriptMap::const_iterator step) {
  if (step == available_scripts_.end()) {
    UpdateRunnableScriptsIfNecessary();
    return;
  }

  Script* script = step->first;
  script->precondition->Check(
      delegate_->GetWebController(), delegate_->GetParameters(),
      executed_scripts_,
      base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                     weak_ptr_factory_.GetWeakPtr(), script, step));
}

void ScriptTracker::OnPreconditionCheck(Script* script,
                                        AvailableScriptMap::const_iterator step,
                                        bool met_preconditions) {
  if (available_scripts_.find(script) == available_scripts_.end()) {
    // Result is not relevant anymore.
    return;
  }
  if (met_preconditions)
    pending_runnable_scripts_.push_back(script);

  RunPreconditionChecksSequentially(++step);
}

void ScriptTracker::ClearAvailableScripts() {
  available_scripts_.clear();
  // Clearing available_scripts_ has cancelled any pending precondition checks,
  // ending them.
  running_checks_ = false;
  pending_runnable_scripts_.clear();
}

}  // namespace autofill_assistant
