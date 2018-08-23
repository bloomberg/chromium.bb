// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"
#include "components/autofill_assistant/browser/actions/assistant_click_action.h"

namespace autofill_assistant {

AssistantClickAction::AssistantClickAction(const AssistantActionProto& proto)
    : AssistantAction(proto) {
  DCHECK(proto_.has_click());
}

AssistantClickAction::~AssistantClickAction() {}

void AssistantClickAction::ProcessAction(AssistantActionDelegate* delegate,
                                         ProcessActionCallback callback) {
  std::vector<std::string> selectors;
  for (const auto& selector : proto_.click().element_to_click().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());
  delegate->ClickElement(selectors, std::move(callback));
}

}  // namespace autofill_assistant.
