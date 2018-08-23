// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_script_executor.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/assistant_protocol_utils.h"
#include "components/autofill_assistant/browser/assistant_service.h"
#include "components/autofill_assistant/browser/assistant_ui_controller.h"
#include "components/autofill_assistant/browser/assistant_web_controller.h"

namespace autofill_assistant {

AssistantScriptExecutor::AssistantScriptExecutor(
    AssistantScript* script,
    AssistantScriptExecutorDelegate* delegate)
    : script_(script), delegate_(delegate), weak_ptr_factory_(this) {
  DCHECK(script_);
  DCHECK(delegate_);
}
AssistantScriptExecutor::~AssistantScriptExecutor() {}

void AssistantScriptExecutor::Run(RunScriptCallback callback) {
  callback_ = std::move(callback);
  DCHECK(delegate_->GetAssistantService());

  delegate_->GetAssistantService()->GetAssistantActions(
      script_->path,
      base::BindOnce(&AssistantScriptExecutor::OnGetAssistantActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistantScriptExecutor::ShowStatusMessage(const std::string& message) {
  delegate_->GetAssistantUiController()->ShowStatusMessage(message);
}

void AssistantScriptExecutor::ClickElement(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->ClickElement(selectors,
                                                       std::move(callback));
}

void AssistantScriptExecutor::ElementExists(
    const std::vector<std::string>& selectors,
    base::OnceCallback<void(bool)> callback) {
  delegate_->GetAssistantWebController()->ElementExists(selectors,
                                                        std::move(callback));
}

void AssistantScriptExecutor::OnGetAssistantActions(
    bool result,
    const std::string& response) {
  if (!result) {
    std::move(callback_).Run(false);
    return;
  }
  processed_actions_.clear();
  actions_.clear();

  bool parse_result = AssistantProtocolUtils::ParseAssistantActions(
      response, &last_server_payload_, &actions_);
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

void AssistantScriptExecutor::ProcessNextAction() {
  if (actions_.empty()) {
    // Request more actions to execute.
    GetNextAssistantActions();
    return;
  }

  std::unique_ptr<AssistantAction> action = std::move(actions_.front());
  actions_.pop_front();
  AssistantAction* action_ptr = action.get();
  action_ptr->ProcessAction(
      this, base::BindOnce(&AssistantScriptExecutor::OnProcessedAction,
                           weak_ptr_factory_.GetWeakPtr(), std::move(action)));
}

void AssistantScriptExecutor::GetNextAssistantActions() {
  delegate_->GetAssistantService()->GetNextAssistantActions(
      last_server_payload_, processed_actions_,
      base::BindOnce(&AssistantScriptExecutor::OnGetAssistantActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistantScriptExecutor::OnProcessedAction(
    std::unique_ptr<AssistantAction> action,
    bool success) {
  processed_actions_.emplace_back();
  ProcessedAssistantActionProto* proto = &processed_actions_.back();
  proto->mutable_action()->MergeFrom(action->proto());
  proto->set_status(success
                        ? ProcessedAssistantActionStatus::ACTION_APPLIED
                        : ProcessedAssistantActionStatus::OTHER_ACTION_STATUS);
  if (!success) {
    // Report error immediately, interrupting action processing.
    GetNextAssistantActions();
    return;
  }
  ProcessNextAction();
}

}  // namespace autofill_assistant
