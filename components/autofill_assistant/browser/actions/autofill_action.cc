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
    : Action(proto), pending_set_field_value_(0), weak_ptr_factory_(this) {
  if (proto.has_use_address()) {
    is_autofill_card_ = false;
    prompt_ = proto.use_address().prompt();
    name_ = proto.use_address().name();
    for (const auto& selector :
         proto.use_address().form_field_element().selectors()) {
      selectors_.emplace_back(selector);
    }
    fill_form_message_ = proto.use_address().strings().fill_form();
    check_form_message_ = proto.use_address().strings().check_form();
  } else {
    DCHECK(proto.has_use_card());
    is_autofill_card_ = true;
    prompt_ = proto.use_card().prompt();
    name_ = "";
    for (const auto& selector :
         proto.use_card().form_field_element().selectors()) {
      selectors_.emplace_back(selector);
    }
    fill_form_message_ = proto.use_card().strings().fill_form();
    check_form_message_ = proto.use_card().strings().check_form();
  }
}

AutofillAction::~AutofillAction() = default;

void AutofillAction::ProcessAction(ActionDelegate* delegate,
                                   ProcessActionCallback action_callback) {
  process_action_callback_ = std::move(action_callback);
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
      delegate->StopCurrentScript(fill_form_message_);
      EndAction(/* successful= */ true);
      return;
    }

    if (selectors_.empty()) {
      // If there is no selector, finish the action directly.
      EndAction(/* successful= */ true);
      return;
    }

    FillFormWithData(guid, delegate);
    return;
  }

  // Show prompt.
  if (!prompt_.empty()) {
    delegate->ShowStatusMessage(prompt_);
  }

  // Ask user to select the data.
  base::OnceCallback<void(const std::string&)> selection_callback =
      base::BindOnce(&AutofillAction::OnDataSelected,
                     weak_ptr_factory_.GetWeakPtr(), delegate);
  if (is_autofill_card_) {
    delegate->ChooseCard(std::move(selection_callback));
    return;
  }

  delegate->ChooseAddress(std::move(selection_callback));
}

void AutofillAction::EndAction(bool successful) {
  UpdateProcessedAction(successful);
  std::move(process_action_callback_).Run(std::move(processed_action_proto_));
}

void AutofillAction::OnDataSelected(ActionDelegate* delegate,
                                    const std::string& guid) {
  // Remember the selection.
  if (is_autofill_card_) {
    delegate->GetClientMemory()->set_selected_card(guid);
  } else {
    delegate->GetClientMemory()->set_selected_address(name_, guid);
  }

  if (selectors_.empty()) {
    // If there is no selector, finish the action directly. This can be the case
    // when we want to trigger the selection of address or card at the beginning
    // of the script and use it later.
    EndAction(/* successful= */ true);
    return;
  }

  if (guid.empty()) {
    // User selected 'Fill manually'.
    delegate->StopCurrentScript(fill_form_message_);
    EndAction(/* successful= */ true);
    return;
  }

  FillFormWithData(guid, delegate);
}

void AutofillAction::FillFormWithData(const std::string& guid,
                                      ActionDelegate* delegate) {
  DCHECK(!selectors_.empty());
  if (is_autofill_card_) {
    delegate->FillCardForm(
        guid, selectors_,
        base::BindOnce(&AutofillAction::OnFormFilled,
                       weak_ptr_factory_.GetWeakPtr(), guid, delegate));
    return;
  }

  delegate->FillAddressForm(
      guid, selectors_,
      base::BindOnce(&AutofillAction::OnFormFilled,
                     weak_ptr_factory_.GetWeakPtr(), guid, delegate));
}

void AutofillAction::OnFormFilled(const std::string& guid,
                                  ActionDelegate* delegate,
                                  bool successful) {
  // In case Autofill failed, we fail the action.
  if (!successful) {
    EndAction(/* successful= */ false);
    return;
  }

  CheckRequiredFields(guid, delegate, /* allow_fallback */ true);
}

void AutofillAction::CheckRequiredFields(const std::string& guid,
                                         ActionDelegate* delegate,
                                         bool allow_fallback) {
  if (is_autofill_card_) {
    // TODO(crbug.com/806868): Implement required fields checking for cards.
    EndAction(/* successful= */ true);
    return;
  }

  // If there are no required fields, finish the action successfully.
  if (proto_.use_address().required_fields().empty()) {
    EndAction(/* successful= */ true);
    return;
  }

  int required_fields_size = proto_.use_address().required_fields_size();
  required_fields_value_status_.clear();
  required_fields_value_status_.resize(required_fields_size, UNKNOWN);
  for (int i = 0; i < required_fields_size; i++) {
    const auto& required_address_field =
        proto_.use_address().required_fields(i);
    DCHECK(required_address_field.has_address_field());
    DCHECK(!required_address_field.element().selectors().empty());
    std::vector<std::string> selectors;
    for (const auto& selector : required_address_field.element().selectors()) {
      selectors.emplace_back(selector);
    }
    delegate->GetFieldValue(
        selectors, base::BindOnce(&AutofillAction::OnGetRequiredFieldValue,
                                  weak_ptr_factory_.GetWeakPtr(), guid,
                                  delegate, allow_fallback, i));
  }
}

void AutofillAction::OnGetRequiredFieldValue(
    const std::string& guid,
    ActionDelegate* delegate,
    bool allow_fallback,
    int index,
    const std::string& value) {
  DCHECK(!is_autofill_card_);
  required_fields_value_status_[index] = value.empty() ? EMPTY : NOT_EMPTY;

  // Wait for the value of all required fields.
  for (const auto& status : required_fields_value_status_) {
    if (status == UNKNOWN) {
      return;
    }
  }

  const autofill::AutofillProfile* profile = delegate->GetAutofillProfile(guid);
  DCHECK(profile);

  // We process all fields with an empty value in order to perform the fallback
  // on all those fields, if any.
  bool validation_successful = true;
  std::vector<std::vector<std::string>> failed_selectors;
  std::vector<std::string> fallback_values;
  for (size_t i = 0; i < required_fields_value_status_.size(); i++) {
    if (required_fields_value_status_[i] == EMPTY) {
      if (!allow_fallback) {
        // Validation failed and we don't want to try the fallback, so we stop
        // the script.
        delegate->StopCurrentScript(check_form_message_);
        EndAction(/* successful= */ true);
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
    EndAction(/* successful= */ true);
    return;
  }

  if (fallback_values.empty()) {
    // One or more required fields is empty but there is no fallback value, so
    // we stop the script.
    delegate->StopCurrentScript(check_form_message_);
    EndAction(/* successful= */ true);
    return;
  }

  pending_set_field_value_ = failed_selectors.size();
  for (size_t i = 0; i < failed_selectors.size(); i++) {
    delegate->SetFieldValue(
        failed_selectors[i], fallback_values[i],
        base::BindOnce(&AutofillAction::OnSetFieldValue,
                       weak_ptr_factory_.GetWeakPtr(), guid, delegate));
  }
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

void AutofillAction::OnSetFieldValue(const std::string& guid,
                                     ActionDelegate* delegate,
                                     bool successful) {
  DCHECK_LT(0u, pending_set_field_value_);
  pending_set_field_value_--;

  // Fail early if filling a field failed and we haven't returned anything yet.
  // We can ignore the other SetFieldValue callbacks given that an action is
  // processed only once.
  if (!successful) {
    // Fallback failed: we stop the script without checking the fields.
    if (process_action_callback_) {
      delegate->StopCurrentScript(check_form_message_);
      EndAction(/* successful= */ true);
    }
    return;
  }

  if (!pending_set_field_value_ && process_action_callback_) {
    // We check the required fields again, but this time we don't want to try
    // the fallback in case if failure.
    CheckRequiredFields(guid, delegate, /* allow_fallback */ false);
  }
}

}  // namespace autofill_assistant
