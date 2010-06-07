// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_data.h"

using webkit_glue::FormData;

namespace {

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
const int kMaxAutocompleteMenuItems = 6;

// The separator characters for credit card values.
const string16 kCreditCardSeparators = ASCIIToUTF16(" -");

}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager(
    TabContents* tab_contents) : tab_contents_(tab_contents),
                                 pending_query_handle_(0),
                                 query_id_(0) {
  DCHECK(tab_contents);

  profile_ = tab_contents_->profile();
  DCHECK(profile_);

  web_data_service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  DCHECK(web_data_service_);

  autofill_enabled_.Init(prefs::kAutoFillEnabled, profile_->GetPrefs(), NULL);
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

void AutocompleteHistoryManager::FormSubmitted(const FormData& form) {
  StoreFormEntriesInWebDatabase(form);
}

bool AutocompleteHistoryManager::GetAutocompleteSuggestions(
    int query_id, const string16& name, const string16& prefix) {
  if (!*autofill_enabled_)
    return false;

  CancelPendingQuery();

  query_id_ = query_id;
  pending_query_handle_ = web_data_service_->GetFormValuesForElementName(
      name, prefix, kMaxAutocompleteMenuItems, this);
  return true;
}

void AutocompleteHistoryManager::RemoveAutocompleteEntry(
    const string16& name, const string16& value) {
  web_data_service_->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (*autofill_enabled_) {
    DCHECK(result);
    SendSuggestions(result);
  } else {
    SendSuggestions(NULL);
  }
}

AutocompleteHistoryManager::AutocompleteHistoryManager(
    Profile* profile, WebDataService* wds) : tab_contents_(NULL),
                                             profile_(profile),
                                             web_data_service_(wds),
                                             pending_query_handle_(0),
                                             query_id_(0) {
  autofill_enabled_.Init(
      prefs::kAutoFillEnabled, profile_->GetPrefs(), NULL);
}

void AutocompleteHistoryManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    SendSuggestions(NULL);
    web_data_service_->CancelRequest(pending_query_handle_);
  }
  pending_query_handle_ = 0;
}

void AutocompleteHistoryManager::StoreFormEntriesInWebDatabase(
    const FormData& form) {
  if (!*autofill_enabled_)
    return;

  if (profile_->IsOffTheRecord())
    return;

  // We put the following restriction on stored FormFields:
  //  - non-empty name
  //  - non-empty value
  //  - text field
  //  - value is not a credit card number
  std::vector<webkit_glue::FormField> values;
  for (std::vector<webkit_glue::FormField>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    if (!iter->value().empty() &&
        !iter->name().empty() &&
        iter->form_control_type() == ASCIIToUTF16("text") &&
        !CreditCard::IsCreditCardNumber(iter->value()))
      values.push_back(*iter);
  }

  if (!values.empty())
    web_data_service_->AddFormFields(values);
}

void AutocompleteHistoryManager::SendSuggestions(const WDTypedResult* result) {
  RenderViewHost* host = tab_contents_->render_view_host();
  if (!host)
    return;

  if (result) {
    DCHECK(result->GetType() == AUTOFILL_VALUE_RESULT);
    const WDResult<std::vector<string16> >* autofill_result =
        static_cast<const WDResult<std::vector<string16> >*>(result);
    host->AutocompleteSuggestionsReturned(
        query_id_, autofill_result->GetValue(), -1);
  } else {
    host->AutocompleteSuggestionsReturned(
        query_id_, std::vector<string16>(), -1);
  }
}
