// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "components/autofill/core/browser/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "components/infobars/core/infobar.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/address_normalizer_factory.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/ssl/insecure_input_tab_helper.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

ChromeAutofillClientIOS::ChromeAutofillClientIOS(
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge,
    password_manager::PasswordGenerationManager* password_generation_manager,
    std::unique_ptr<IdentityProvider> identity_provider)
    : pref_service_(browser_state->GetPrefs()),
      personal_data_manager_(PersonalDataManagerFactory::GetForBrowserState(
          browser_state->GetOriginalChromeBrowserState())),
      web_state_(web_state),
      bridge_(bridge),
      identity_provider_(std::move(identity_provider)),
      autofill_web_data_service_(
          ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
              browser_state,
              ServiceAccessType::EXPLICIT_ACCESS)),
      infobar_manager_(infobar_manager),
      password_generation_manager_(password_generation_manager),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

ChromeAutofillClientIOS::~ChromeAutofillClientIOS() {
  HideAutofillPopup();
}

PersonalDataManager* ChromeAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

PrefService* ChromeAutofillClientIOS::GetPrefs() {
  return pref_service_;
}

// TODO(crbug.com/535784): Implement this when adding credit card upload.
syncer::SyncService* ChromeAutofillClientIOS::GetSyncService() {
  return nullptr;
}

IdentityProvider* ChromeAutofillClientIOS::GetIdentityProvider() {
  return identity_provider_.get();
}

ukm::UkmRecorder* ChromeAutofillClientIOS::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
}

AddressNormalizer* ChromeAutofillClientIOS::GetAddressNormalizer() {
  if (base::FeatureList::IsEnabled(features::kAutofillAddressNormalizer))
    return AddressNormalizerFactory::GetInstance();
  return nullptr;
}

SaveCardBubbleController*
ChromeAutofillClientIOS::GetSaveCardBubbleController() {
  return nullptr;
}

void ChromeAutofillClientIOS::ShowAutofillSettings() {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      // autofill::CardUnmaskPromptViewBridge manages its own lifetime, so
      // do not use std::unique_ptr<> here.
      new autofill::CardUnmaskPromptViewBridge(&unmask_controller_), card,
      reason, delegate);
}

void ChromeAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    const base::Closure& callback) {
  // This method is invoked synchronously from
  // AutofillManager::OnFormSubmitted(); at the time of detecting that a form
  // was submitted, the WebContents is guaranteed to be live. Since the
  // InfoBarService is a WebContentsUserData, it must also be alive at this
  // time.
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          false, card, std::unique_ptr<base::DictionaryValue>(nullptr),
          callback, GetPrefs())));
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_cvc_be_requested,
    const base::Closure& callback) {
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      std::make_unique<AutofillSaveCardInfoBarDelegateMobile>(
          true, card, std::move(legal_message), callback, GetPrefs())));
}

void ChromeAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    const base::Closure& callback) {
  auto infobar_delegate =
      std::make_unique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, callback);
  auto* raw_delegate = infobar_delegate.get();
  if (infobar_manager_->AddInfoBar(
          ::CreateConfirmInfoBar(std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
}

void ChromeAutofillClientIOS::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  callback.Run(ios::GetChromeBrowserProvider()->GetRiskData());
}

bool ChromeAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void ChromeAutofillClientIOS::ScanCreditCard(
    const CreditCardScanCallback& callback) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:suggestions popupDelegate:delegate];
}

void ChromeAutofillClientIOS::HideAutofillPopup() {
  [bridge_ hideAutofillPopup];
}

bool ChromeAutofillClientIOS::IsAutocompleteEnabled() {
  // For browser, Autocomplete is always enabled as part of Autofill.
  return GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void ChromeAutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

void ChromeAutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  if (password_generation_manager_) {
    password_generation_manager_->DetectFormsEligibleForGeneration(forms);
  }
}

void ChromeAutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

scoped_refptr<AutofillWebDataService> ChromeAutofillClientIOS::GetDatabase() {
  return autofill_web_data_service_;
}

void ChromeAutofillClientIOS::DidInteractWithNonsecureCreditCardInput() {
  InsecureInputTabHelper::GetOrCreateForWebState(web_state_)
      ->DidInteractWithNonsecureCreditCardInput();
}

bool ChromeAutofillClientIOS::IsContextSecure() {
  return IsContextSecureForWebState(web_state_);
}

bool ChromeAutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

void ChromeAutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

bool ChromeAutofillClientIOS::IsAutofillSupported() {
  return true;
}

}  // namespace autofill
