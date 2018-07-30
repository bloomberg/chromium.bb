// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_wait_for_dom_action.h"

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantWaitForDomAction::AssistantWaitForDomAction(
    int timeout_ms,
    const std::vector<std::string>& selectors,
    bool for_absence)
    : target_element_selectors_(selectors) {}

AssistantWaitForDomAction::~AssistantWaitForDomAction() {}

void AssistantWaitForDomAction::ProcessAction(AssistantActionDelegate* delegate,
                                              ProcessActionCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace autofill_assistant.
