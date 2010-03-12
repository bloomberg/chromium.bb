// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/string_util.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "webkit/glue/form_field_values.h"

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
static const int kMaxAutofillMenuItems = 6;

AutocompleteHistoryManager::AutocompleteHistoryManager(
    TabContents* tab_contents) : tab_contents_(tab_contents),
                                 pending_query_handle_(0),
                                 query_id_(0) {
  form_autofill_enabled_.Init(prefs::kAutoFillEnabled,
      profile()->GetPrefs(), NULL);
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

void AutocompleteHistoryManager::CancelPendingQuery() {
  if (pending_query_handle_) {
    SendSuggestions(NULL);
    WebDataService* web_data_service =
        profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
    if (!web_data_service) {
      NOTREACHED();
      return;
    }
    web_data_service->CancelRequest(pending_query_handle_);
  }
  pending_query_handle_ = 0;
}

Profile* AutocompleteHistoryManager::profile() {
  return tab_contents_->profile();
}

void AutocompleteHistoryManager::FormFieldValuesSubmitted(
    const webkit_glue::FormFieldValues& form) {
  StoreFormEntriesInWebDatabase(form);
}

bool AutocompleteHistoryManager::GetAutocompleteSuggestions(
    int query_id, const string16& name, const string16& prefix) {
  if (!*form_autofill_enabled_)
    return false;

  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return false;
  }

  CancelPendingQuery();

  query_id_ = query_id;

  pending_query_handle_ = web_data_service->GetFormValuesForElementName(
      name, prefix, kMaxAutofillMenuItems, this);
  return true;
}

void AutocompleteHistoryManager::RemoveAutocompleteEntry(
    const string16& name, const string16& value) {
  WebDataService* web_data_service =
      profile()->GetWebDataService(Profile::EXPLICIT_ACCESS);
  if (!web_data_service) {
    NOTREACHED();
    return;
  }

  web_data_service->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
    const WDTypedResult* result) {
  DCHECK(pending_query_handle_);
  pending_query_handle_ = 0;

  if (*form_autofill_enabled_) {
    DCHECK(result);
    SendSuggestions(result);
  } else {
    SendSuggestions(NULL);
  }
}

void AutocompleteHistoryManager::StoreFormEntriesInWebDatabase(
    const webkit_glue::FormFieldValues& form) {
  if (!*form_autofill_enabled_)
    return;

  if (profile()->IsOffTheRecord())
    return;

  // We put the following restriction on stored FormFields:
  //  - input_type() == WebInputElement::Text
  //  - non-empty name
  //  - non-empty value
  //  - non-empty form_control_type()
  std::vector<webkit_glue::FormField> values;
  for (std::vector<webkit_glue::FormField>::const_iterator iter =
           form.elements.begin();
       iter != form.elements.end(); ++iter) {
    if (iter->input_type() == WebKit::WebInputElement::Text &&
        !iter->value().empty() &&
        !iter->name().empty() &&
        !iter->form_control_type().empty())
      values.push_back(*iter);
  }

  profile()->GetWebDataService(Profile::EXPLICIT_ACCESS)->
      AddFormFieldValues(values);
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
