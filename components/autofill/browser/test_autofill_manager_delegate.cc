// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/test_autofill_manager_delegate.h"

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

bool TestAutofillManagerDelegate::IsSavingPasswordsEnabled() const {
  return false;
}

bool TestAutofillManagerDelegate::IsPasswordSyncEnabled() const {
  return false;
}

void TestAutofillManagerDelegate::SetSyncStateChangedCallback(
    const base::Closure& callback) { }

void TestAutofillManagerDelegate::OnAutocheckoutError() {}

void TestAutofillManagerDelegate::ShowAutofillSettings() {}

void TestAutofillManagerDelegate::ConfirmSaveCreditCard(
    const AutofillMetrics& metric_logger,
    const CreditCard& credit_card,
    const base::Closure& save_card_callback) {}

void TestAutofillManagerDelegate::ShowPasswordGenerationBubble(
    const gfx::Rect& bounds,
    const content::PasswordForm& form,
    autofill::PasswordGenerator* generator) {}

void TestAutofillManagerDelegate::ShowAutocheckoutBubble(
    const gfx::RectF& bounding_box,
    const gfx::NativeView& native_view,
    const base::Callback<void(bool)>& callback) {}

void TestAutofillManagerDelegate::HideAutocheckoutBubble() {}

void TestAutofillManagerDelegate::ShowRequestAutocompleteDialog(
    const FormData& form,
    const GURL& source_url,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback) {}

void TestAutofillManagerDelegate::RequestAutocompleteDialogClosed() {}

void TestAutofillManagerDelegate::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& identifiers,
    AutofillPopupDelegate* delegate) {}

void TestAutofillManagerDelegate::HideAutofillPopup() {}

void TestAutofillManagerDelegate::UpdateProgressBar(double value) {}

}  // namespace autofill
