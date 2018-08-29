// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_card_action.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UseCardAction::UseCardAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {}

UseCardAction::~UseCardAction() {}

void UseCardAction::ProcessAction(ActionDelegate* delegate,
                                  ProcessActionCallback callback) {
  delegate->ChooseCard(base::BindOnce(&UseCardAction::OnChooseCard,
                                      weak_ptr_factory_.GetWeakPtr(), delegate,
                                      std::move(callback)));
}

void UseCardAction::OnChooseCard(ActionDelegate* delegate,
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
      base::BindOnce(&UseCardAction::OnFillCardForm,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UseCardAction::OnFillCardForm(ProcessActionCallback callback,
                                   bool result) {
  std::move(callback).Run(result);
}

}  // namespace autofill_assistant.
