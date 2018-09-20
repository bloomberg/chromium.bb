// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"

namespace autofill_assistant {

AutofillAction::AutofillAction(const ActionProto& proto)
    : Action(proto), weak_ptr_factory_(this) {
  if (proto.has_use_address()) {
    prompt_ = proto.use_address().prompt();
    name_ = proto.use_address().name();
    for (const auto& selector :
         proto.use_address().form_field_element().selectors()) {
      selectors_.emplace_back(selector);
    }
  } else {
    DCHECK(proto.has_use_card());
    is_autofill_card_ = true;
    prompt_ = proto.use_card().prompt();
    for (const auto& selector :
         proto.use_card().form_field_element().selectors()) {
      selectors_.emplace_back(selector);
    }
  }
}

AutofillAction::~AutofillAction() = default;

void AutofillAction::ProcessAction(ActionDelegate* delegate,
                                   ProcessActionCallback action_callback) {
  processed_action_proto_ = std::make_unique<ProcessedActionProto>();
  // Check data already selected in a previous action.
  base::Optional<std::string> selected_data;
  if (is_autofill_card_) {
    selected_data = delegate->GetClientMemory()->selected_card();
  } else {
    selected_data = delegate->GetClientMemory()->selected_address(name_);
  }

  if (selected_data) {
    const std::string& guid = selected_data.value();
    if (guid.empty()) {
      // User selected 'Fill manually'.
      // TODO(crbug.com/806868): We need to differentiate between action failure
      // and stopping an action to let the user fill a form (expected stop).
      UpdateProcessedAction(false);
      std::move(action_callback).Run(std::move(processed_action_proto_));
      return;
    }

    if (selectors_.empty()) {
      // If there is no selector, finish the action directly.
      UpdateProcessedAction(true);
      std::move(action_callback).Run(std::move(processed_action_proto_));
      return;
    }

    FillFormWithData(guid, delegate, std::move(action_callback));
    return;
  }

  // Show prompt.
  if (!prompt_.empty()) {
    delegate->ShowStatusMessage(prompt_);
  }

  // Ask user to select the data.
  base::OnceCallback<void(const std::string&)> selection_callback =
      base::BindOnce(&AutofillAction::OnDataSelected,
                     weak_ptr_factory_.GetWeakPtr(), delegate,
                     std::move(action_callback));
  if (is_autofill_card_) {
    delegate->ChooseCard(std::move(selection_callback));
    return;
  }

  delegate->ChooseAddress(std::move(selection_callback));
}

void AutofillAction::OnDataSelected(ActionDelegate* delegate,
                                    ProcessActionCallback action_callback,
                                    const std::string& guid) {
  // Remember the selection.
  if (is_autofill_card_) {
    delegate->GetClientMemory()->set_selected_card(guid);
  } else {
    delegate->GetClientMemory()->set_selected_address(name_, guid);
  }

  if (guid.empty()) {
    // User selected 'Fill manually'.
    // TODO(crbug.com/806868): We need to differentiate between action failure
    // and stopping an action to let the user fill a form (expected stop).
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  if (selectors_.empty()) {
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

void AutofillAction::FillFormWithData(const std::string& guid,
                                      ActionDelegate* delegate,
                                      ProcessActionCallback action_callback) {
  DCHECK(!selectors_.empty());
  if (is_autofill_card_) {
    delegate->FillAddressForm(
        guid, selectors_,
        base::BindOnce(&AutofillAction::OnFormFilled,
                       weak_ptr_factory_.GetWeakPtr(), guid, delegate,
                       std::move(action_callback)));
    return;
  }

  delegate->FillCardForm(guid, selectors_,
                         base::BindOnce(&AutofillAction::OnFormFilled,
                                        weak_ptr_factory_.GetWeakPtr(), guid,
                                        delegate, std::move(action_callback)));
}

void AutofillAction::OnFormFilled(const std::string& guid,
                                  ActionDelegate* delegate,
                                  ProcessActionCallback action_callback,
                                  bool successful) {
  // In case Autofill failed, we stop the action.
  if (!successful) {
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    // TODO(crbug.com/806868): Tell the user to fill the form manually.
    return;
  }

  CheckRequiredFields(guid, delegate, std::move(action_callback),
                      /* allow_fallback */ true);
}

void AutofillAction::CheckRequiredFields(const std::string& guid,
                                         ActionDelegate* delegate,
                                         ProcessActionCallback action_callback,
                                         bool allow_fallback) {
  if (is_autofill_card_) {
    // TODO(crbug.com/806868): Implement required fields checking for cards.
    UpdateProcessedAction(true);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  // If there are no required fields, finish the action successfully.
  if (proto_.use_address().required_fields().empty()) {
    UpdateProcessedAction(true);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  std::vector<std::vector<std::string>> selectors_list;
  for (const auto& required_address_field :
       proto_.use_address().required_fields()) {
    selectors_list.emplace_back(std::vector<std::string>());
    DCHECK(required_address_field.has_address_field());
    DCHECK(!required_address_field.element().selectors().empty());
    for (const auto& selector : required_address_field.element().selectors()) {
      selectors_list.back().emplace_back(selector);
    }
  }

  delegate->GetFieldsValue(
      selectors_list,
      base::BindOnce(&AutofillAction::OnGetRequiredFieldsValue,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate,
                     std::move(action_callback), allow_fallback));
}

void AutofillAction::OnGetRequiredFieldsValue(
    const std::string& guid,
    ActionDelegate* delegate,
    ProcessActionCallback action_callback,
    bool allow_fallback,
    const std::vector<std::string>& values) {
  DCHECK(!is_autofill_card_);
  DCHECK_EQ(proto_.use_address().required_fields_size(),
            static_cast<int>(values.size()));

  const autofill::AutofillProfile* profile = delegate->GetAutofillProfile(guid);
  DCHECK(profile);

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool validation_successful = true;
  std::vector<std::vector<std::string>> failed_selectors;
  std::vector<std::string> fallback_values;
  for (size_t i = 0; i < values.size(); i++) {
    if (values[i].empty()) {
      if (!allow_fallback) {
        // Validation failed and we don't want to try the fallback, so we fail
        // the action.
        UpdateProcessedAction(false);
        std::move(action_callback).Run(std::move(processed_action_proto_));
        // TODO(crbug.com/806868): Tell the user to fill the form manually.
        return;
      }

      validation_successful = false;
      std::string fallback_value = base::UTF16ToUTF8(GetAddressFieldValue(
          profile, proto_.use_address().required_fields(i).address_field()));
      if (fallback_value.empty()) {
        // If there is no fallback value, we skip this failed field.
        continue;
      }

      fallback_values.emplace_back(fallback_value);
      failed_selectors.emplace_back(std::vector<std::string>());
      for (const auto& selector :
           proto_.use_address().required_fields(i).element().selectors()) {
        failed_selectors.back().emplace_back(selector);
      }
    }
  }

  DCHECK_EQ(failed_selectors.size(), fallback_values.size());

  if (validation_successful) {
    UpdateProcessedAction(true);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    return;
  }

  if (fallback_values.empty()) {
    // One or more required fields is empty but there is no fallback value, so
    // we fail the action.
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    // TODO(crbug.com/806868): Tell the user to fill the form manually.
    return;
  }

  delegate->SetFieldsValue(
      failed_selectors, fallback_values,
      base::BindOnce(&AutofillAction::OnSetFieldsValue,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate,
                     std::move(action_callback)));
}

base::string16 AutofillAction::GetAddressFieldValue(
    const autofill::AutofillProfile* profile,
    const UseAddressProto::RequiredField::AddressField& address_field) {
  // TODO(crbug.com/806868): Get the actual application locale.
  std::string app_locale = "en-US";
  switch (address_field) {
    case UseAddressProto::RequiredField::FIRST_NAME:
      return profile->GetInfo(autofill::NAME_FIRST, app_locale);
    case UseAddressProto::RequiredField::LAST_NAME:
      return profile->GetInfo(autofill::NAME_LAST, app_locale);
    case UseAddressProto::RequiredField::FULL_NAME:
      return profile->GetInfo(autofill::NAME_FULL, app_locale);
    case UseAddressProto::RequiredField::PHONE_NUMBER:
      return profile->GetInfo(autofill::PHONE_HOME_WHOLE_NUMBER, app_locale);
    case UseAddressProto::RequiredField::EMAIL:
      return profile->GetInfo(autofill::EMAIL_ADDRESS, app_locale);
    case UseAddressProto::RequiredField::ORGANIZATION:
      return profile->GetInfo(autofill::COMPANY_NAME, app_locale);
    case UseAddressProto::RequiredField::COUNTRY_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_COUNTRY, app_locale);
    case UseAddressProto::RequiredField::REGION:
      return profile->GetInfo(autofill::ADDRESS_HOME_STATE, app_locale);
    case UseAddressProto::RequiredField::STREET_ADDRESS:
      return profile->GetInfo(autofill::ADDRESS_HOME_STREET_ADDRESS,
                              app_locale);
    case UseAddressProto::RequiredField::LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_CITY, app_locale);
    case UseAddressProto::RequiredField::DEPENDANT_LOCALITY:
      return profile->GetInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
                              app_locale);
    case UseAddressProto::RequiredField::POSTAL_CODE:
      return profile->GetInfo(autofill::ADDRESS_HOME_ZIP, app_locale);
    case UseAddressProto::RequiredField::UNDEFINED:
      NOTREACHED();
      return base::string16();
  }
}

void AutofillAction::OnSetFieldsValue(const std::string& guid,
                                      ActionDelegate* delegate,
                                      ProcessActionCallback action_callback,
                                      bool successful) {
  if (!successful) {
    // Fallback failed: we fail the action without checking the fields.
    UpdateProcessedAction(false);
    std::move(action_callback).Run(std::move(processed_action_proto_));
    // TODO(crbug.com/806868): Tell the user to fill the form manually.
    return;
  }

  // We check the required fields again, but this time we don't want to try the
  // fallback in case if failure.
  CheckRequiredFields(guid, delegate, std::move(action_callback),
                      /* allow_fallback */ false);
}

}  // namespace autofill_assistant.
