// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "components/autofill_assistant/browser/service.h"
#include "components/autofill_assistant/browser/ui_controller.h"
#include "components/autofill_assistant/browser/web_controller.h"

namespace autofill_assistant {

ScriptExecutor::ScriptExecutor(const std::string& script_path,
                               ScriptExecutorDelegate* delegate)
    : script_path_(script_path), delegate_(delegate), weak_ptr_factory_(this) {
  DCHECK(delegate_);
}
ScriptExecutor::~ScriptExecutor() {}

void ScriptExecutor::Run(RunScriptCallback callback) {
  callback_ = std::move(callback);
  DCHECK(delegate_->GetService());

  delegate_->GetService()->GetActions(
      script_path_, base::BindOnce(&ScriptExecutor::OnGetActions,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::ShowStatusMessage(const std::string& message) {
  delegate_->GetUiController()->ShowStatusMessage(message);
}

void ScriptExecutor::ClickElement(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->ClickElement(selectors, std::move(callback));
}

void ScriptExecutor::ElementExists(const std::vector<std::string>& selectors,
                                   base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->ElementExists(selectors, std::move(callback));
}

void ScriptExecutor::ChooseAddress(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetUiController()->ChooseAddress(std::move(callback));
}

void ScriptExecutor::FillAddressForm(const std::string& guid,
                                     const std::vector<std::string>& selectors,
                                     base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillAddressForm(guid, selectors,
                                                 std::move(callback));
}

void ScriptExecutor::ChooseCard(
    base::OnceCallback<void(const std::string&)> callback) {
  delegate_->GetUiController()->ChooseCard(std::move(callback));
}

void ScriptExecutor::FillCardForm(const std::string& guid,
                                  const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FillCardForm(guid, selectors,
                                              std::move(callback));
}

void ScriptExecutor::SelectOption(const std::vector<std::string>& selectors,
                                  const std::string& selected_option,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SelectOption(selectors, selected_option,
                                              std::move(callback));
}

void ScriptExecutor::FocusElement(const std::vector<std::string>& selectors,
                                  base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->FocusElement(selectors, std::move(callback));
}

void ScriptExecutor::GetFieldsValue(
    const std::vector<std::vector<std::string>>& selectors_list,
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  delegate_->GetWebController()->GetFieldsValue(selectors_list,
                                                std::move(callback));
}

void ScriptExecutor::SetFieldsValue(
    const std::vector<std::vector<std::string>>& selectors_list,
    const std::vector<std::string>& values,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetWebController()->SetFieldsValue(selectors_list, values,
                                                std::move(callback));
}

const autofill::AutofillProfile* ScriptExecutor::GetAutofillProfile(
    const std::string& guid) {
  // TODO(crbug.com/806868): Implement GetAutofillProfile.
  return nullptr;
}

ClientMemory* ScriptExecutor::GetClientMemory() {
  return delegate_->GetClientMemory();
}

void ScriptExecutor::OnGetActions(bool result, const std::string& response) {
  if (!result) {
    std::move(callback_).Run(false);
    return;
  }
  processed_actions_.clear();
  actions_.clear();

  bool parse_result =
      ProtocolUtils::ParseActions(response, &last_server_payload_, &actions_);
  if (!parse_result) {
    std::move(callback_).Run(false);
    return;
  }

  if (actions_.empty()) {
    // Finished executing the script if there are no more actions.
    std::move(callback_).Run(true);
    return;
  }
  ProcessNextAction();
}

void ScriptExecutor::ProcessNextAction() {
  if (actions_.empty()) {
    // Request more actions to execute.
    GetNextActions();
    return;
  }

  std::unique_ptr<Action> action = std::move(actions_.front());
  actions_.pop_front();
  int delay_ms = action->proto().action_delay_ms();
  if (delay_ms > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ScriptExecutor::ProcessAction,
                       weak_ptr_factory_.GetWeakPtr(), std::move(action)),
        base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    ProcessAction(std::move(action));
  }
}

void ScriptExecutor::ProcessAction(std::unique_ptr<Action> action) {
  action->ProcessAction(this, base::BindOnce(&ScriptExecutor::OnProcessedAction,
                                             weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::GetNextActions() {
  delegate_->GetService()->GetNextActions(
      last_server_payload_, processed_actions_,
      base::BindOnce(&ScriptExecutor::OnGetActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ScriptExecutor::OnProcessedAction(
    std::unique_ptr<ProcessedActionProto> processed_action_proto) {
  processed_actions_.emplace_back(*processed_action_proto);
  if (processed_actions_.back().status() !=
      ProcessedActionStatus::ACTION_APPLIED) {
    // Report error immediately, interrupting action processing.
    GetNextActions();
    return;
  }
  ProcessNextAction();
}

}  // namespace autofill_assistant
