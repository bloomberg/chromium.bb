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

// The separator characters for SSNs.
const string16 kSSNSeparators = ASCIIToUTF16(" -");

bool IsSSN(const string16& text) {
  string16 number_string;
  RemoveChars(text, kSSNSeparators.c_str(), &number_string);
  if (number_string.length() != 9 || !IsStringASCII(number_string))
    return false;

  // A SSN is of the form AAA-GG-SSSS (A = area number, G = group number, S =
  // serial number). The validation we do here is simply checking if the area,
  // group, and serial numbers are valid. It is possible to check if the group
  // number is valid for the given area, but that data changes all the time.
  //
  // See: http://www.socialsecurity.gov/history/ssn/geocard.html
  //      http://www.socialsecurity.gov/employer/stateweb.htm
  //      http://www.socialsecurity.gov/employer/ssnvhighgroup.htm

  string16 area_string = number_string.substr(0, 3);
  string16 group_string = number_string.substr(3, 2);
  string16 serial_string = number_string.substr(5, 4);

  int area = StringToInt(area_string);
  if (area < 1 ||
      area == 666 ||
      (area > 733 && area < 750) ||
      area > 772)
    return false;

  int group = StringToInt(group_string);
  if (group == 0)
    return false;

  int serial = StringToInt(serial_string);
  if (serial == 0)
    return false;

  return true;
}

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
  //  - value is not a SSN
  std::vector<webkit_glue::FormField> values;
  for (std::vector<webkit_glue::FormField>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    if (!iter->value().empty() &&
        !iter->name().empty() &&
        iter->form_control_type() == ASCIIToUTF16("text") &&
        !CreditCard::IsCreditCardNumber(iter->value()) &&
        !IsSSN(iter->value()))
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
        query_id_, autofill_result->GetValue());
  } else {
    host->AutocompleteSuggestionsReturned(
        query_id_, std::vector<string16>());
  }
}
