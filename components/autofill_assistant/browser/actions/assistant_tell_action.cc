// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_tell_action.h"

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantTellAction::AssistantTellAction(const std::string& message)
    : message_(message) {}

AssistantTellAction::~AssistantTellAction() {}

void AssistantTellAction::ProcessAction(AssistantActionDelegate* delegate,
                                        ProcessActionCallback callback) {
  delegate->ShowStatusMessage(message_);
  std::move(callback).Run(true);
}

}  // namespace autofill_assistant.