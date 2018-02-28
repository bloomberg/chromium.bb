// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"

namespace autofill {
namespace {

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
const int kMaxAutocompleteMenuItems = 6;

bool IsTextField(const FormFieldData& field) {
  return
      field.form_control_type == "text" ||
      field.form_control_type == "search" ||
      field.form_control_type == "tel" ||
      field.form_control_type == "url" ||
      field.form_control_type == "email";
}

}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager(
    AutofillDriver* driver,
    AutofillClient* autofill_client)
    : driver_(driver),
      database_(autofill_client->GetDatabase()),
      pending_query_handle_(0),
      query_id_(0),
      external_delegate_(nullptr),
      autofill_client_(autofill_client) {
  DCHECK(autofill_client_);
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

void AutocompleteHistoryManager::OnGetAutocompleteSuggestions(
    int query_id,
    const base::string16& name,
    const base::string16& prefix,
    const std::string& form_control_type) {
  CancelPendingQuery();

  query_id_ = query_id;
  if (!autofill_client_->IsAutocompleteEnabled() ||
      !autofill_client_->IsAutofillSupported() ||
      form_control_type == "textarea" ||
      IsInAutofillSuggestionsDisabledExperiment()) {
    SendSuggestions(nullptr);
    return;
  }

  if (database_.get()) {
    pending_query_handle_ = database_->GetFormValuesForElementName(
        name, prefix, kMaxAutocompleteMenuItems, this);
  }
}

void AutocompleteHistoryManager::OnWillSubmitForm(const FormData& form) {
  if (!autofill_client_->IsAutocompleteEnabled())
    return;

  if (driver_->IsIncognito())
    return;

  // We put the following restriction on stored FormFields:
  //  - non-empty name
  //  - non-empty value
  //  - text field
  //  - autocomplete is not disabled
  //  - value is not a credit card number
  //  - value is not a SSN
  //  - field was not identified as a CVC field (this is handled in
  //    AutofillManager)
  std::vector<FormFieldData> values;
  for (const FormFieldData& field : form.fields) {
    if (!field.value.empty() &&
        !field.name.empty() &&
        IsTextField(field) &&
        field.should_autocomplete &&
        !IsValidCreditCardNumber(field.value) &&
        !IsSSN(field.value)) {
      values.push_back(field);
    }
  }

  if (!values.empty() && database_.get())
    database_->AddFormFields(values);
}

void AutocompleteHistoryManager::OnRemoveAutocompleteEntry(
    const base::string16& name, const base::string16& value) {
  if (database_.get())
    database_->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::SetExternalDelegate(
    AutofillExternalDelegate* delegate) {
  external_delegate_ = delegate;
}

void AutocompleteHistoryManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    if (database_.get())
      database_->CancelRequest(pending_query_handle_);
    pending_query_handle_ = 0;
  }
}

void AutocompleteHistoryManager::SendSuggestions(
    const std::vector<base::string16>* autocomplete_results) {
  std::vector<Suggestion> suggestions;
  if (autocomplete_results) {
    for (const auto& result : *autocomplete_results) {
      suggestions.push_back(Suggestion(result));
    }
  }

  external_delegate_->OnSuggestionsReturned(query_id_, suggestions);
  query_id_ = 0;
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  DCHECK(result);
  // Returning early here if |result| is NULL.  We've seen this happen on
  // Linux due to NFS dismounting and causing sql failures.
  // See http://crbug.com/68783.
  if (!result) {
    SendSuggestions(nullptr);
    return;
  }

  DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
  const WDResult<std::vector<base::string16>>* autofill_result =
      static_cast<const WDResult<std::vector<base::string16>>*>(result.get());
  std::vector<base::string16> suggestions = autofill_result->GetValue();
  SendSuggestions(&suggestions);
}

}  // namespace autofill
