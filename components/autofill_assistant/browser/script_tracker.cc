// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

namespace {

// Sort scripts by priority.
void SortScripts(std::vector<Script*>* scripts) {
  std::sort(scripts->begin(), scripts->end(),
            [](const Script* a, const Script* b) {
              // Runnable scripts with lowest priority value are displayed
              // first, Interrupts with lowest priority values are run first.
              // Order of scripts with the same priority is arbitrary. Fallback
              // to ordering by name and path, arbitrarily, for the behavior to
              // be consistent across runs.
              return std::tie(a->priority, a->handle.name, a->handle.path) <
                     std::tie(b->priority, b->handle.name, a->handle.path);
            });
}

}  // namespace

ScriptTracker::ScriptTracker(ScriptExecutorDelegate* delegate,
                             ScriptTracker::Listener* listener)
    : delegate_(delegate),
      listener_(listener),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetScripts(std::vector<std::unique_ptr<Script>> scripts) {
  // The set of interrupts must not change while a script is running, as they're
  // passed to ScriptExecutor.
  if (running())
    return;

  TerminatePendingChecks();

  available_scripts_.clear();
  for (auto& script : scripts) {
    available_scripts_[script.get()] = std::move(script);
  }

  interrupts_.clear();
  for (auto& entry : available_scripts_) {
    Script* script = entry.first;
    if (script->handle.interrupt) {
      interrupts_.push_back(script);
    }
  }
  SortScripts(&interrupts_);
}

void ScriptTracker::CheckScripts() {
  // In case checks are still running, terminate them.
  TerminatePendingChecks();

  DCHECK(pending_runnable_scripts_.empty());

  GURL url = delegate_->GetCurrentURL();
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (const auto& entry : available_scripts_) {
    Script* script = entry.first;
    if (script->handle.name.empty() &&
        script->handle.chip_icon == ChipIcon::NO_ICON &&
        !script->handle.autostart)
      continue;

    script->precondition->Check(
        url, batch_element_checker_.get(),
        delegate_->GetTriggerContext()->script_parameters, scripts_state_,
        base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                       weak_ptr_factory_.GetWeakPtr(), script));
  }
  if (batch_element_checker_->empty() && pending_runnable_scripts_.empty() &&
      !available_scripts_.empty()) {
    DVLOG(1) << __func__ << ": No runnable scripts for " << url << " out of "
             << available_scripts_.size() << " available.";
    // There are no runnable scripts, even though we haven't checked the DOM
    // yet. Report it all immediately.
    UpdateRunnableScriptsIfNecessary();
    listener_->OnNoRunnableScripts();
    TerminatePendingChecks();
    return;
  }
  batch_element_checker_->Run(delegate_->GetWebController(),
                              base::BindOnce(&ScriptTracker::OnCheckDone,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ScriptTracker::ExecuteScript(const std::string& script_path,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
    DVLOG(1) << "Do not expect executing the script (" << script_path
             << " when there is a script running.";
    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executor_ = std::make_unique<ScriptExecutor>(
      script_path, last_global_payload_, last_script_payload_,
      /* listener= */ this, &scripts_state_, &interrupts_, delegate_);
  ScriptExecutor::RunScriptCallback run_script_callback = base::BindOnce(
      &ScriptTracker::OnScriptRun, weak_ptr_factory_.GetWeakPtr(), script_path,
      std::move(callback));
  TerminatePendingChecks();
  executor_->Run(std::move(run_script_callback));
}

void ScriptTracker::ClearRunnableScripts() {
  runnable_scripts_.clear();
}

void ScriptTracker::OnScriptRun(
    const std::string& script_path,
    ScriptExecutor::RunScriptCallback original_callback,
    const ScriptExecutor::Result& result) {
  executor_.reset();
  MaybeSwapInScripts();
  std::move(original_callback).Run(result);
}

void ScriptTracker::MaybeSwapInScripts() {
  if (scripts_update_) {
    SetScripts(std::move(*scripts_update_));
    scripts_update_.reset();
  }
}

void ScriptTracker::OnCheckDone() {
  UpdateRunnableScriptsIfNecessary();
  TerminatePendingChecks();
}

void ScriptTracker::UpdateRunnableScriptsIfNecessary() {
  if (!RunnablesHaveChanged())
    return;

  runnable_scripts_.clear();
  SortScripts(&pending_runnable_scripts_);
  for (Script* script : pending_runnable_scripts_) {
    runnable_scripts_.push_back(script->handle);
  }

  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::TerminatePendingChecks() {
  batch_element_checker_.reset();
  pending_runnable_scripts_.clear();
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

void ScriptTracker::OnPreconditionCheck(Script* script,
                                        bool met_preconditions) {
  if (available_scripts_.find(script) == available_scripts_.end()) {
    // Result is not relevant anymore.
    return;
  }
  if (met_preconditions)
    pending_runnable_scripts_.push_back(script);
}

void ScriptTracker::OnServerPayloadChanged(const std::string& global_payload,
                                           const std::string& script_payload) {
  last_global_payload_ = global_payload;
  last_script_payload_ = script_payload;
}

void ScriptTracker::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  scripts_update_.reset(
      new std::vector<std::unique_ptr<Script>>(std::move(scripts)));
}

}  // namespace autofill_assistant
