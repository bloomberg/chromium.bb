// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete_history_manager.h"

#include <vector>

#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/autofill_messages.h"
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

  // A SSN is of the form AAA-GG-SSSS (A = area number, G = group number, S =
  // serial number). The validation we do here is simply checking if the area,
  // group, and serial numbers are valid. It is possible to check if the group
  // number is valid for the given area, but that data changes all the time.
  //
  // See: http://www.socialsecurity.gov/history/ssn/geocard.html
  //      http://www.socialsecurity.gov/employer/stateweb.htm
  //      http://www.socialsecurity.gov/employer/ssnvhighgroup.htm
  if (number_string.length() != 9 || !IsStringASCII(number_string))
    return false;

  int area;
  if (!base::StringToInt(number_string.begin(),
                         number_string.begin() + 3,
                         &area))
    return false;
  if (area < 1 ||
      area == 666 ||
      (area > 733 && area < 750) ||
      area > 772)
    return false;

  int group;
  if (!base::StringToInt(number_string.begin() + 3,
                         number_string.begin() + 5,
                         &group) || group == 0)
    return false;

  int serial;
  if (!base::StringToInt(number_string.begin() + 5,
                         number_string.begin() + 9,
                         &serial) || serial == 0)
    return false;

  return true;
}

}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager(
    TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      pending_query_handle_(0),
      query_id_(0) {
  profile_ = tab_contents->profile();
  // May be NULL in unit tests.
  web_data_service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  autofill_enabled_.Init(prefs::kAutoFillEnabled, profile_->GetPrefs(), NULL);
}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelPendingQuery();
}

bool AutocompleteHistoryManager::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutocompleteHistoryManager, message)
    IPC_MESSAGE_HANDLER(AutoFillHostMsg_RemoveAutocompleteEntry,
                        OnRemoveAutocompleteEntry)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutocompleteHistoryManager::OnFormSubmitted(const FormData& form) {
  if (!*autofill_enabled_)
    return;

  if (profile_->IsOffTheRecord())
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

  if (!values.empty() && web_data_service_.get())
    web_data_service_->AddFormFields(values);
}

void AutocompleteHistoryManager::OnRemoveAutocompleteEntry(
    const string16& name, const string16& value) {
  if (web_data_service_.get())
    web_data_service_->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::OnGetAutocompleteSuggestions(
    int query_id,
    const string16& name,
    const string16& prefix,
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
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

  if (web_data_service_.get()) {
    pending_query_handle_ = web_data_service_->GetFormValuesForElementName(
        name, prefix, kMaxAutocompleteMenuItems, this);
  }
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataService::Handle h,
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
  const WDResult<std::vector<string16> >* autofill_result =
      static_cast<const WDResult<std::vector<string16> >*>(result);
  std::vector<string16> suggestions = autofill_result->GetValue();
  SendSuggestions(&suggestions);
}

AutocompleteHistoryManager::AutocompleteHistoryManager(
    TabContents* tab_contents,
    Profile* profile,
    WebDataService* wds)
    : TabContentsObserver(tab_contents),
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
    if (web_data_service_.get())
      web_data_service_->CancelRequest(pending_query_handle_);
    pending_query_handle_ = 0;
  }
}

void AutocompleteHistoryManager::SendSuggestions(
    const std::vector<string16>* suggestions) {
  if (suggestions) {
    // Combine AutoFill and Autocomplete values into values and labels.
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
        autofill_labels_.push_back(string16());
        autofill_icons_.push_back(string16());
        autofill_unique_ids_.push_back(0);  // 0 means no profile.
      }
    }
  }

  Send(new AutoFillMsg_SuggestionsReturned(routing_id(),
                                           query_id_,
                                           autofill_values_,
                                           autofill_labels_,
                                           autofill_icons_,
                                           autofill_unique_ids_));

  query_id_ = 0;
  autofill_values_.clear();
  autofill_labels_.clear();
  autofill_icons_.clear();
  autofill_unique_ids_.clear();
}
