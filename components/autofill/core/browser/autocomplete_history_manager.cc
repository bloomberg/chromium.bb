// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"

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
      external_delegate_(NULL),
      autofill_client_(autofill_client) {
  DCHECK(autofill_client_);
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  // TODO(robliao): Remove ScopedTracker below once https://crbug.com/422460 is
  // fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "422460 AutocompleteHistoryManager::OnWebDataServiceRequestDone"));

  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (!autofill_client_->IsAutocompleteEnabled()) {
    SendSuggestions(NULL);
    return;
  }

  DCHECK(result);
  // Returning early here if |result| is NULL.  We've seen this happen on
  // Linux due to NFS dismounting and causing sql failures.
  // See http://crbug.com/68783.
  if (!result) {
    SendSuggestions(NULL);
    return;
  }

  DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
  const WDResult<std::vector<base::string16> >* autofill_result =
      static_cast<const WDResult<std::vector<base::string16> >*>(result);
  std::vector<base::string16> suggestions = autofill_result->GetValue();
  SendSuggestions(&suggestions);
}

void AutocompleteHistoryManager::OnGetAutocompleteSuggestions(
    int query_id,
    const base::string16& name,
    const base::string16& prefix,
    const std::string& form_control_type,
    const std::vector<Suggestion>& suggestions) {
  CancelPendingQuery();

  query_id_ = query_id;
  autofill_suggestions_ = suggestions;
  if (!autofill_client_->IsAutocompleteEnabled() ||
      form_control_type == "textarea") {
    SendSuggestions(NULL);
    return;
  }

  if (database_.get()) {
    pending_query_handle_ = database_->GetFormValuesForElementName(
        name, prefix, kMaxAutocompleteMenuItems, this);
  }
}

void AutocompleteHistoryManager::OnFormSubmitted(const FormData& form) {
  if (!autofill_client_->IsAutocompleteEnabled())
    return;

  if (driver_->IsOffTheRecord())
    return;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return;

  // We put the following restriction on stored FormFields:
  //  - non-empty name
  //  - non-empty value
  //  - text field
  //  - autocomplete is not disabled
  //  - value is not a credit card number
  //  - value is not a SSN
  std::vector<FormFieldData> values;
  for (std::vector<FormFieldData>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    if (!iter->value.empty() &&
        !iter->name.empty() &&
        IsTextField(*iter) &&
        iter->should_autocomplete &&
        !autofill::IsValidCreditCardNumber(iter->value) &&
        !autofill::IsSSN(iter->value)) {
      values.push_back(*iter);
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
  if (autocomplete_results) {
    // Combine Autofill and Autocomplete values into values and labels.
    for (const auto& new_result : *autocomplete_results) {
      bool unique = true;
      for (const auto& autofill_suggestion : autofill_suggestions_) {
        // Don't add duplicate values.
        if (new_result == autofill_suggestion.value) {
          unique = false;
          break;
        }
      }

      if (unique)
        autofill_suggestions_.push_back(Suggestion(new_result));
    }
  }

  external_delegate_->OnSuggestionsReturned(query_id_, autofill_suggestions_);

  query_id_ = 0;
  autofill_suggestions_.clear();
}

}  // namespace autofill
