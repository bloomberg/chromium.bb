// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"
#if !defined(OS_ANDROID)
#include "components/autofill/core/browser/ui/mock_save_card_bubble_controller.h"
#endif

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

TestAutofillClient::TestAutofillClient()
    : token_service_(new FakeOAuth2TokenService()),
      identity_provider_(new FakeIdentityProvider(token_service_.get())),
      rappor_service_(new rappor::TestRapporServiceImpl()),
#if !defined(OS_ANDROID)
      save_card_bubble_controller_(new MockSaveCardBubbleController()),
#endif
      form_origin_(GURL("https://example.test")) {}

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

syncer::SyncService* TestAutofillClient::GetSyncService() {
  return nullptr;
}

IdentityProvider* TestAutofillClient::GetIdentityProvider() {
  return identity_provider_.get();
}

rappor::RapporServiceImpl* TestAutofillClient::GetRapporServiceImpl() {
  return rappor_service_.get();
}

ukm::UkmRecorder* TestAutofillClient::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

SaveCardBubbleController* TestAutofillClient::GetSaveCardBubbleController() {
#if defined(OS_ANDROID)
  return nullptr;
#else
  return save_card_bubble_controller_.get();
#endif
}

void TestAutofillClient::ShowAutofillSettings() {
}

void TestAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
}

void TestAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
}

void TestAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    const base::Closure& callback) {
}

void TestAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_cvc_be_requested,
    const base::Closure& callback) {
  callback.Run();
}

void TestAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    const base::Closure& callback) {
  callback.Run();
}

void TestAutofillClient::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  callback.Run("some risk data");
}

bool TestAutofillClient::HasCreditCardScanFeature() {
  return false;
}

void TestAutofillClient::ScanCreditCard(
    const CreditCardScanCallback& callback) {
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

void TestAutofillClient::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
}

void TestAutofillClient::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
}

bool TestAutofillClient::IsContextSecure() {
  // Simplified secure context check for tests.
  return form_origin_.SchemeIs("https");
}

bool TestAutofillClient::ShouldShowSigninPromo() {
  return false;
}

void TestAutofillClient::StartSigninFlow() {}

void TestAutofillClient::ShowHttpNotSecureExplanation() {}

}  // namespace autofill
