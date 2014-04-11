// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

TestAutofillManagerDelegate::TestAutofillManagerDelegate() {}
TestAutofillManagerDelegate::~TestAutofillManagerDelegate() {}

PersonalDataManager* TestAutofillManagerDelegate::GetPersonalDataManager() {
  return NULL;
}

scoped_refptr<AutofillWebDataService>
TestAutofillManagerDelegate::GetDatabase() {
  return scoped_refptr<AutofillWebDataService>(NULL);
}

PrefService* TestAutofillManagerDelegate::GetPrefs() { return prefs_.get(); }

void TestAutofillManagerDelegate::HideRequestAutocompleteDialog() {}

void TestAutofillManagerDelegate::ShowAutofillSettings() {}

void TestAutofillManagerDelegate::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const base::Closure& save_card_callback) {}

void TestAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const ResultCallback& callback) {}

void TestAutofillManagerDelegate::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels,
    const std::vector<base::string16>& icons,
    const std::vector<int>& identifiers,
    base::WeakPtr<AutofillPopupDelegate> delegate) {}

void TestAutofillManagerDelegate::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {}

void TestAutofillManagerDelegate::HideAutofillPopup() {}

bool TestAutofillManagerDelegate::IsAutocompleteEnabled() {
  return true;
}

void TestAutofillManagerDelegate::DetectAccountCreationForms(
    const std::vector<autofill::FormStructure*>& forms) {}

void TestAutofillManagerDelegate::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

}  // namespace autofill
