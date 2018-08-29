// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"

namespace autofill_assistant {

UseAddressAction::UseAddressAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {}

UseAddressAction::~UseAddressAction() {}

void UseAddressAction::ProcessAction(ActionDelegate* delegate,
                                     ProcessActionCallback callback) {
  if (proto_.use_address().has_usage()) {
    delegate->ShowStatusMessage(proto_.use_address().usage());
  }

  delegate->ChooseAddress(base::BindOnce(&UseAddressAction::OnChooseAddress,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         delegate, std::move(callback)));
}

void UseAddressAction::OnChooseAddress(ActionDelegate* delegate,
                                       ProcessActionCallback callback,
                                       const std::string& guid) {
  if (guid.empty()) {
    DVLOG(1) << "Failed to choose address.";
    std::move(callback).Run(false);
    return;
  }

  std::vector<std::string> selectors;
  for (const auto& selector :
       proto_.use_address().form_field_element().selectors()) {
    selectors.emplace_back(selector);
  }
  DCHECK(!selectors.empty());
  delegate->FillAddressForm(
      guid, selectors,
      base::BindOnce(&UseAddressAction::OnFillAddressForm,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UseAddressAction::OnFillAddressForm(ProcessActionCallback callback,
                                         bool result) {
  std::move(callback).Run(result);
}

}  // namespace autofill_assistant.
