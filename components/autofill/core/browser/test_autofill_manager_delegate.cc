// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_manager_delegate.h"

namespace autofill {

TestAutofillManagerDelegate::TestAutofillManagerDelegate() {}
TestAutofillManagerDelegate::~TestAutofillManagerDelegate() {}

PersonalDataManager* TestAutofillManagerDelegate::GetPersonalDataManager() {
  return NULL;
}

autocheckout::WhitelistManager*
TestAutofillManagerDelegate::GetAutocheckoutWhitelistManager() const {
  return NULL;
}

PrefService* TestAutofillManagerDelegate::GetPrefs() {
  return NULL;
}

void TestAutofillManagerDelegate::HideRequestAutocompleteDialog() {}

void TestAutofillManagerDelegate::OnAutocheckoutError() {}

void TestAutofillManagerDelegate::OnAutocheckoutSuccess() {}

void TestAutofillManagerDelegate::ShowAutofillSettings() {}

void TestAutofillManagerDelegate::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const CreditCard& credit_card,
    const base::Closure& save_card_callback) {}

bool TestAutofillManagerDelegate::ShowAutocheckoutBubble(
    const gfx::RectF& bounding_box,
    bool is_google_user,
    const base::Callback<void(AutocheckoutBubbleState)>& callback) {
  return true;
}

void TestAutofillManagerDelegate::HideAutocheckoutBubble() {}

void TestAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback) {}

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

void TestAutofillManagerDelegate::AddAutocheckoutStep(
    AutocheckoutStepType step_type) {}

void TestAutofillManagerDelegate::UpdateAutocheckoutStep(
    AutocheckoutStepType step_type,
    AutocheckoutStepStatus step_status) {}

bool TestAutofillManagerDelegate::IsAutocompleteEnabled() {
  return true;
}

}  // namespace autofill
