// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

TestAutofillClient::TestAutofillClient() {
}
TestAutofillClient::~TestAutofillClient() {
}

PersonalDataManager* TestAutofillClient::GetPersonalDataManager() {
  return NULL;
}

scoped_refptr<AutofillWebDataService> TestAutofillClient::GetDatabase() {
  return scoped_refptr<AutofillWebDataService>(NULL);
}

PrefService* TestAutofillClient::GetPrefs() {
  return prefs_.get();
}

void TestAutofillClient::HideRequestAutocompleteDialog() {
}

void TestAutofillClient::ShowAutofillSettings() {
}

void TestAutofillClient::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const base::Closure& save_card_callback) {
}

void TestAutofillClient::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const ResultCallback& callback) {
}

void TestAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels,
    const std::vector<base::string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
}

void TestAutofillClient::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
}

void TestAutofillClient::HideAutofillPopup() {
}

bool TestAutofillClient::IsAutocompleteEnabled() {
  return true;
}

void TestAutofillClient::DetectAccountCreationForms(
    const std::vector<autofill::FormStructure*>& forms) {
}

void TestAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
}

}  // namespace autofill
