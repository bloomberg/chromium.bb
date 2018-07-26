// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_script_executor.h"

#include "base/bind.h"
#include "components/autofill_assistant/browser/assistant_protocol_utils.h"
#include "components/autofill_assistant/browser/assistant_service.h"

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
      base::BindOnce(&AssistantScriptExecutor::onGetAssistantActions,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AssistantScriptExecutor::onGetAssistantActions(
    bool result,
    const std::string& response) {
  if (!result) {
    std::move(callback_).Run(false);
    return;
  }

  DCHECK(!response.empty());
  actions_ = AssistantProtocolUtils::ParseAssistantActions(
      response, &last_server_payload_);
  // TODO(crbug.com/806868): Check parsed actions and perform them.
  std::move(callback_).Run(true);
}

}  // namespace autofill_assistant
