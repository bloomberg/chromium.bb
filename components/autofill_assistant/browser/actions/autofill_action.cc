// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"

namespace autofill_assistant {

AutofillAction::Data::Data() = default;
AutofillAction::Data::~Data() = default;

AutofillAction::AutofillAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  if (proto.has_use_address()) {
    data_.prompt = proto.use_address().prompt();
    data_.name = proto.use_address().name();
    for (const auto& selector :
         proto.use_address().form_field_element().selectors()) {
      data_.selectors.emplace_back(selector);
    }
    for (const auto& required_address_field :
         proto.use_address().required_fields()) {
      // Some UseAddressProto::RequiredField don't have a selector to only
      // specify an address_field, in which case we ignore it.
      if (required_address_field.element().selectors().empty()) {
        continue;
      }

      data_.required_fields_selectors.emplace_back(std::vector<std::string>());
      for (const auto& selector :
           required_address_field.element().selectors()) {
        data_.required_fields_selectors.back().emplace_back(selector);
      }
    }
  } else {
    // TODO(crbug.com/806868): Also read the required fields when filling a
    // credit card form.
    DCHECK(proto.has_use_card());
    data_.is_autofill_card = true;
    data_.prompt = proto.use_card().prompt();
    for (const auto& selector :
         proto.use_card().form_field_element().selectors()) {
      data_.selectors.emplace_back(selector);
    }
  }
}

AutofillAction::~AutofillAction() = default;

void AutofillAction::ProcessAction(ActionDelegate* delegate,
                                   ProcessActionCallback action_callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  // Check data already selected in a previous action.
  base::Optional<std::string> selected_data;
  if (data_.is_autofill_card) {
    selected_data = delegate->GetClientMemory()->selected_card();
  } else {
    selected_data = delegate->GetClientMemory()->selected_address(data_.name);
  }

  if (selected_data) {
    std::string guid = selected_data.value();
    if (guid.empty()) {
      // User selected 'Fill manually'.
      // TODO(crbug.com/806868): We need to differentiate between action failure
      // and stopping an action to let the user fill a form (expected stop).
      UpdateProcessedAction(false);
      std::move(action_callback).Run(std::move(processed_action_proto_));
      return;
    }

    if (data_.selectors.empty()) {
      // If there is no selector, finish the action directly.
      UpdateProcessedAction(true);
      std::move(action_callback).Run(std::move(processed_action_proto_));
      return;
    }

    FillFormWithData(guid, delegate, std::move(action_callback));
    return;
  }

  // Show prompt.
  std::string prompt = data_.prompt;
  if (!prompt.empty()) {
    delegate->ShowStatusMessage(prompt);
  }

  // Ask user to select the data.
  base::OnceCallback<void(const std::string&)> selection_callback =
      base::BindOnce(&AutofillAction::OnDataSelected,
                     weak_ptr_factory_.GetWeakPtr(), delegate,
                     std::move(action_callback));
  if (data_.is_autofill_card) {
    delegate->ChooseCard(std::move(selection_callback));
    return;
  }

  delegate->ChooseAddress(std::move(selection_callback));
}

void AutofillAction::OnDataSelected(ActionDelegate* delegate,
                                    ProcessActionCallback action_callback,
                                    const std::string& guid) {
  // Remember the selection.
  if (data_.is_autofill_card) {
    delegate->GetClientMemory()->set_selected_card(guid);
  } else {
    delegate->GetClientMemory()->set_selected_address(data_.name, guid);
  }

  if (guid.empty()) {
    // User selected 'Fill manually'.
    // TODO(crbug.com/806868): We need to differentiate between action failure
    // and stopping an action to let the user fill a form (expected stop).
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  if (data_.selectors.empty()) {
    // If there is no selector, finish the action directly. This can be the case
    // when we want to trigger the selection of address or card at the beginning
    // of the script and use it later.
    UpdateProcessedAction(true);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  // TODO(crbug.com/806868): Validate form and use fallback if needed.
  FillFormWithData(guid, delegate, std::move(action_callback));
}

void AutofillAction::FillFormWithData(std::string guid,
                                      ActionDelegate* delegate,
                                      ProcessActionCallback action_callback) {
  DCHECK(!data_.selectors.empty());
  if (data_.is_autofill_card) {
    delegate->FillAddressForm(
        guid, data_.selectors,
        base::BindOnce(&AutofillAction::OnFormFilled,
                       weak_ptr_factory_.GetWeakPtr(), delegate,
                       std::move(action_callback)));
    return;
  }
  delegate->FillCardForm(guid, data_.selectors,
                         base::BindOnce(&AutofillAction::OnFormFilled,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        delegate, std::move(action_callback)));
}

void AutofillAction::OnFormFilled(ActionDelegate* delegate,
                                  ProcessActionCallback action_callback,
                                  bool successful) {
  // In case Autofill failed, we stop the action.
  if (!successful) {
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    // TODO(crbug.com/806868): Tell the user to fill the form manually.
    return;
  }

  // If there are no required fields, finish the action successfully.
  if (data_.required_fields_selectors.empty()) {
    UpdateProcessedAction(true);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  // Otherwise, we check that all required fields have a value.
  delegate->GetFieldsValue(
      data_.required_fields_selectors,
      base::BindOnce(&AutofillAction::OnGetRequiredFieldsValue,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(action_callback)));
}

void AutofillAction::OnGetRequiredFieldsValue(
    ProcessActionCallback action_callback,
    const std::vector<std::string>& values) {
  for (const std::string& value : values) {
    if (value.empty()) {
      // Validation failed: at least one required field is empty.
      // TODO(crbug.com/806868): Tell the user to fill the form manually.
      // TODO(crbug.com/806868): Use fallback and retry validation.
      UpdateProcessedAction(false);
      std::move(action_callback).Run(std::move(processed_action_proto_));
      return;
    }
  }

  UpdateProcessedAction(true);
  std::move(action_callback).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant.
