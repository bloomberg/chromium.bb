// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/assistant_use_card_action.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/assistant_action_delegate.h"

namespace autofill_assistant {

AssistantUseCardAction::AssistantUseCardAction(const ActionProto& proto)
    : AssistantAction(proto), weak_ptr_factory_(this) {}

AssistantUseCardAction::~AssistantUseCardAction() {}

void AssistantUseCardAction::ProcessAction(AssistantActionDelegate* delegate,
                                           ProcessActionCallback callback) {
  delegate->ChooseCard(base::BindOnce(&AssistantUseCardAction::OnChooseCard,
                                      weak_ptr_factory_.GetWeakPtr(), delegate,
                                      std::move(callback)));
}

void AssistantUseCardAction::OnChooseCard(AssistantActionDelegate* delegate,
                                          ProcessActionCallback callback,
                                          const std::string& guid) {
  if (guid.empty()) {
    DVLOG(1) << "Failed to choose card.";
    std::move(callback).Run(false);
    return;
  }

  std::vector<std::string> selectors;
  for (const auto& selector :
       proto_.use_card().form_field_element().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());
  delegate->FillCardForm(
      guid, selectors,
      base::BindOnce(&AssistantUseCardAction::OnFillCardForm,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void AssistantUseCardAction::OnFillCardForm(ProcessActionCallback callback,
                                            bool result) {
  std::move(callback).Run(result);
}

}  // namespace autofill_assistant.
