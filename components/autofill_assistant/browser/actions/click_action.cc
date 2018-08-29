// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/click_action.h"

#include <utility>

#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

ClickAction::ClickAction(const ActionProto& proto) : Action(proto) {
  DCHECK(proto_.has_click());
}

ClickAction::~ClickAction() {}

void ClickAction::ProcessAction(ActionDelegate* delegate,
                                ProcessActionCallback callback) {
  std::vector<std::string> selectors;
  for (const auto& selector : proto_.click().element_to_click().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());
  delegate->ClickElement(selectors, std::move(callback));
}

}  // namespace autofill_assistant.
