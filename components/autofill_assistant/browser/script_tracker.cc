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

namespace autofill_assistant {

ScriptTracker::ScriptTracker(ScriptExecutorDelegate* delegate,
                             ScriptTracker::Listener* listener)
    : delegate_(delegate),
      listener_(listener),
      reported_runnable_scripts_(false),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetScripts(std::vector<std::unique_ptr<Script>> scripts) {
  ClearAvailableScripts();
  for (auto& script : scripts) {
    available_scripts_[script.get()] = std::move(script);
  }
}

void ScriptTracker::CheckScripts(const base::TimeDelta& max_duration) {
  // In case checks are still running, terminate them.
  TerminatePendingChecks();

  DCHECK(pending_runnable_scripts_.empty());

  batch_element_checker_ =
      delegate_->GetWebController()->CreateBatchElementChecker();
  for (const auto& entry : available_scripts_) {
    Script* script = entry.first;
    script->precondition->Check(
        delegate_->GetWebController()->GetUrl(), batch_element_checker_.get(),
        delegate_->GetParameters(), executed_scripts_,
        base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                       weak_ptr_factory_.GetWeakPtr(), script));
  }
  if (batch_element_checker_->all_found() &&
      pending_runnable_scripts_.empty() && reported_runnable_scripts_) {
    // There are no runnable scripts, even though we haven't checked the DOM
    // yet. Report it all immediately.
    UpdateRunnableScriptsIfNecessary();
    listener_->OnNoRunnableScriptsAnymore();
    OnCheckDone();
    return;
  }
  batch_element_checker_->Run(
      max_duration,
      /* try_done= */
      base::BindRepeating(&ScriptTracker::UpdateRunnableScriptsIfNecessary,
                          base::Unretained(this)),
      /* all_done= */
      base::BindOnce(&ScriptTracker::OnCheckDone, base::Unretained(this)));
  // base::Unretained(this) is safe since this instance owns
  // batch_element_checker_.
}

void ScriptTracker::ExecuteScript(const std::string& script_path,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
    DLOG(ERROR) << "Do not expect executing the script (" << script_path
                << " when there is a script running.";
    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executed_scripts_[script_path] = SCRIPT_STATUS_RUNNING;
  executor_ = std::make_unique<ScriptExecutor>(
      script_path, last_server_payload_, this, delegate_);
  ScriptExecutor::RunScriptCallback run_script_callback = base::BindOnce(
      &ScriptTracker::OnScriptRun, weak_ptr_factory_.GetWeakPtr(), script_path,
      std::move(callback));
  TerminatePendingChecks();
  executor_->Run(std::move(run_script_callback));
}

void ScriptTracker::ClearRunnableScripts() {
  runnable_scripts_.clear();
}

base::Value ScriptTracker::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  std::string last_server_payload_js = last_server_payload_;
  base::Base64Encode(last_server_payload_js, &last_server_payload_js);
  dict.SetKey("last-payload", base::Value(last_server_payload_js));

  std::vector<base::Value> executed_scripts_js;
  for (const auto& entry : executed_scripts_) {
    base::Value script_js = base::Value(base::Value::Type::DICTIONARY);
    script_js.SetKey(entry.first, base::Value(entry.second));
    executed_scripts_js.push_back(std::move(script_js));
  }
  dict.SetKey("executed-scripts", base::Value(executed_scripts_js));

  std::vector<base::Value> available_scripts_js;
  for (const auto& entry : available_scripts_)
    available_scripts_js.push_back(base::Value(entry.second->handle.path));
  dict.SetKey("available-scripts", base::Value(available_scripts_js));

  std::vector<base::Value> runnable_scripts_js;
  for (const auto& entry : runnable_scripts_) {
    base::Value script_js = base::Value(base::Value::Type::DICTIONARY);
    script_js.SetKey("name", base::Value(entry.name));
    script_js.SetKey("path", base::Value(entry.path));
    script_js.SetKey("initial_prompt", base::Value(entry.initial_prompt));
    script_js.SetKey("autostart", base::Value(entry.autostart));
    script_js.SetKey("highlight", base::Value(entry.highlight));
    runnable_scripts_js.push_back(std::move(script_js));
  }
  dict.SetKey("runnable-scripts", base::Value(runnable_scripts_js));

  return dict;
}

void ScriptTracker::OnScriptRun(
    const std::string& script_path,
    ScriptExecutor::RunScriptCallback original_callback,
    ScriptExecutor::Result result) {
  executor_.reset();
  executed_scripts_[script_path] =
      result.success ? SCRIPT_STATUS_SUCCESS : SCRIPT_STATUS_FAILURE;
  std::move(original_callback).Run(result);
}

void ScriptTracker::UpdateRunnableScriptsIfNecessary() {
  if (!RunnablesHaveChanged())
    return;

  runnable_scripts_.clear();
  std::sort(pending_runnable_scripts_.begin(), pending_runnable_scripts_.end(),
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

  reported_runnable_scripts_ = true;
  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::OnCheckDone() {
  TerminatePendingChecks();
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

void ScriptTracker::ClearAvailableScripts() {
  available_scripts_.clear();
  // Clearing available_scripts_ has cancelled any pending precondition checks,
  // ending them.
  TerminatePendingChecks();
}

void ScriptTracker::OnServerPayloadChanged(const std::string& server_payload) {
  last_server_payload_ = server_payload;
}

}  // namespace autofill_assistant
