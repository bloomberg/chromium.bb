// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_external_delegate.h"
#include "components/autofill/browser/validation.h"
#include "components/autofill/common/autofill_messages.h"
#include "components/autofill/common/autofill_pref_names.h"
#include "components/autofill/common/form_data.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::BrowserContext;
using content::WebContents;

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
      field.form_control_type == "email" ||
      field.form_control_type == "text";
}

}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager(
    WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      autofill_data_(
          AutofillWebDataService::FromBrowserContext(browser_context_)),
      pending_query_handle_(0),
      query_id_(0),
      external_delegate_(NULL) {
  autofill_enabled_.Init(
      prefs::kAutofillEnabled,
      components::UserPrefs::Get(browser_context_));
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

bool AutocompleteHistoryManager::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutocompleteHistoryManager, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_RemoveAutocompleteEntry,
                        OnRemoveAutocompleteEntry)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (!*autofill_enabled_) {
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
    const std::vector<base::string16>& autofill_values,
    const std::vector<base::string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  CancelPendingQuery();

  query_id_ = query_id;
  autofill_values_ = autofill_values;
  autofill_labels_ = autofill_labels;
  autofill_icons_ = autofill_icons;
  autofill_unique_ids_ = autofill_unique_ids;
  if (!*autofill_enabled_) {
    SendSuggestions(NULL);
    return;
  }

  if (autofill_data_.get()) {
    pending_query_handle_ = autofill_data_->GetFormValuesForElementName(
        name, prefix, kMaxAutocompleteMenuItems, this);
  }
}

void AutocompleteHistoryManager::OnFormSubmitted(const FormData& form) {
  if (!*autofill_enabled_)
    return;

  if (browser_context_->IsOffTheRecord())
    return;

  // Don't save data that was submitted through JavaScript.
  if (!form.user_submitted)
    return;

  // We put the following restriction on stored FormFields:
  //  - non-empty name
  //  - non-empty value
  //  - text field
  //  - value is not a credit card number
  //  - value is not a SSN
  std::vector<FormFieldData> values;
  for (std::vector<FormFieldData>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    if (!iter->value.empty() &&
        !iter->name.empty() &&
        IsTextField(*iter) &&
        !autofill::IsValidCreditCardNumber(iter->value) &&
        !autofill::IsSSN(iter->value)) {
      values.push_back(*iter);
    }
  }

  if (!values.empty() && autofill_data_.get())
    autofill_data_->AddFormFields(values);
}

void AutocompleteHistoryManager::OnRemoveAutocompleteEntry(
    const base::string16& name, const base::string16& value) {
  if (autofill_data_.get())
    autofill_data_->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::SetExternalDelegate(
    AutofillExternalDelegate* delegate) {
  external_delegate_ = delegate;
}

void AutocompleteHistoryManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    if (autofill_data_)
      autofill_data_->CancelRequest(pending_query_handle_);
    pending_query_handle_ = 0;
  }
}

void AutocompleteHistoryManager::SendSuggestions(
    const std::vector<base::string16>* suggestions) {
  if (suggestions) {
    // Combine Autofill and Autocomplete values into values and labels.
    for (size_t i = 0; i < suggestions->size(); ++i) {
      bool unique = true;
      for (size_t j = 0; j < autofill_values_.size(); ++j) {
        // Don't add duplicate values.
        if (autofill_values_[j] == (*suggestions)[i]) {
          unique = false;
          break;
        }
      }

      if (unique) {
        autofill_values_.push_back((*suggestions)[i]);
        autofill_labels_.push_back(base::string16());
        autofill_icons_.push_back(base::string16());
        autofill_unique_ids_.push_back(0);  // 0 means no profile.
      }
    }
  }

  if (external_delegate_) {
    external_delegate_->OnSuggestionsReturned(
        query_id_,
        autofill_values_,
        autofill_labels_,
        autofill_icons_,
        autofill_unique_ids_);
  } else {
    Send(new AutofillMsg_SuggestionsReturned(routing_id(),
                                             query_id_,
                                             autofill_values_,
                                             autofill_labels_,
                                             autofill_icons_,
                                             autofill_unique_ids_));
  }

  query_id_ = 0;
  autofill_values_.clear();
  autofill_labels_.clear();
  autofill_icons_.clear();
  autofill_unique_ids_.clear();
}

}  // namespace autofill
