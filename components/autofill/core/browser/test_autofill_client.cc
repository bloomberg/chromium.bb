// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

TestAutofillClient::TestAutofillClient()
    : token_service_(new FakeOAuth2TokenService()),
      identity_provider_(new FakeIdentityProvider(token_service_.get())),
      rappor_service_(new rappor::TestRapporService()) {
}
TestAutofillClient::~TestAutofillClient() {
}

PersonalDataManager* TestAutofillClient::GetPersonalDataManager() {
  return nullptr;
}

scoped_refptr<AutofillWebDataService> TestAutofillClient::GetDatabase() {
  return scoped_refptr<AutofillWebDataService>(nullptr);
}

PrefService* TestAutofillClient::GetPrefs() {
  return prefs_.get();
}

IdentityProvider* TestAutofillClient::GetIdentityProvider() {
  return identity_provider_.get();
}

rappor::RapporService* TestAutofillClient::GetRapporService() {
  return rappor_service_.get();
}

void TestAutofillClient::HideRequestAutocompleteDialog() {
}

void TestAutofillClient::ShowAutofillSettings() {
}

void TestAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
}

void TestAutofillClient::OnUnmaskVerificationResult(GetRealPanResult result) {
}

void TestAutofillClient::ConfirmSaveCreditCard(
    const base::Closure& save_card_callback) {
}

bool TestAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void TestAutofillClient::ScanCreditCard(
    const CreditCardScanCallback& callback) {
}

void TestAutofillClient::ShowRequestAutocompleteDialog(
    const FormData& form,
    content::RenderFrameHost* rfh,
    const ResultCallback& callback) {
}

void TestAutofillClient::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
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
    content::RenderFrameHost* rfh,
    const std::vector<autofill::FormStructure*>& forms) {
}

void TestAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
}

void TestAutofillClient::OnFirstUserGestureObserved() {
}

void TestAutofillClient::LinkClicked(const GURL& url,
                                     WindowOpenDisposition disposition) {
}

}  // namespace autofill
