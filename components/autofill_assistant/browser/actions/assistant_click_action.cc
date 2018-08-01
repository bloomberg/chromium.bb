// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_click_action.h"

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantClickAction::AssistantClickAction(
    const std::vector<std::string>& selectors)
    : target_element_selectors_(selectors) {}

AssistantClickAction::~AssistantClickAction() {}

void AssistantClickAction::ProcessAction(AssistantActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  delegate->ClickElement(target_element_selectors_, std::move(callback));
}

}  // namespace autofill_assistant.