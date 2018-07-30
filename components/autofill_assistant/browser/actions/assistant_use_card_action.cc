// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_use_card_action.h"

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantUseCardAction::AssistantUseCardAction(
    const std::vector<std::string>& selectors)
    : target_element_selectors_(selectors) {}

AssistantUseCardAction::~AssistantUseCardAction() {}

void AssistantUseCardAction::ProcessAction(AssistantActionDelegate* delegate,
                                           ProcessActionCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill_assistant.
