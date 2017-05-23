// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/identity_provider.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

AutofillClientIOS::AutofillClientIOS(
    ios::ChromeBrowserState* browser_state,
    web::WebState* web_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge,
    password_manager::PasswordGenerationManager* password_generation_manager,
    std::unique_ptr<IdentityProvider> identity_provider)
    : browser_state_(browser_state),
      web_state_(web_state),
      infobar_manager_(infobar_manager),
      bridge_(bridge),
      password_generation_manager_(password_generation_manager),
      identity_provider_(std::move(identity_provider)),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

AutofillClientIOS::~AutofillClientIOS() {
  HideAutofillPopup();
}

PersonalDataManager* AutofillClientIOS::GetPersonalDataManager() {
  return autofill::PersonalDataManagerFactory::GetForBrowserState(
      browser_state_->GetOriginalChromeBrowserState());
}

PrefService* AutofillClientIOS::GetPrefs() {
  return browser_state_->GetPrefs();
}

// TODO(crbug.com/535784): Implement this when adding credit card upload.
syncer::SyncService* AutofillClientIOS::GetSyncService() {
  return nullptr;
}

IdentityProvider* AutofillClientIOS::GetIdentityProvider() {
  return identity_provider_.get();
}

rappor::RapporServiceImpl* AutofillClientIOS::GetRapporServiceImpl() {
  return GetApplicationContext()->GetRapporServiceImpl();
}

ukm::UkmRecorder* AutofillClientIOS::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
}

SaveCardBubbleController* AutofillClientIOS::GetSaveCardBubbleController() {
  return nullptr;
}

void AutofillClientIOS::ShowAutofillSettings() {
  NOTREACHED();
}

void AutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      // autofill::CardUnmaskPromptViewBridge manages its own lifetime, so
      // do not use std::unique_ptr<> here.
      new autofill::CardUnmaskPromptViewBridge(&unmask_controller_), card,
      reason, delegate);
}

void AutofillClientIOS::OnUnmaskVerificationResult(PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}

void AutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    const base::Closure& callback) {
  // This method is invoked synchronously from
  // AutofillManager::OnFormSubmitted(); at the time of detecting that a form
  // was submitted, the WebContents is guaranteed to be live. Since the
  // InfoBarService is a WebContentsUserData, it must also be alive at this
  // time.
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      base::MakeUnique<AutofillSaveCardInfoBarDelegateMobile>(
          false, card, std::unique_ptr<base::DictionaryValue>(nullptr),
          callback, GetPrefs())));
}

void AutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_cvc_be_requested,
    const base::Closure& callback) {
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      base::MakeUnique<AutofillSaveCardInfoBarDelegateMobile>(
          true, card, std::move(legal_message), callback, GetPrefs())));
}

void AutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    const base::Closure& callback) {
  auto infobar_delegate =
      base::MakeUnique<AutofillCreditCardFillingInfoBarDelegateMobile>(
          card, callback);
  auto* raw_delegate = infobar_delegate.get();
  if (infobar_manager_->AddInfoBar(
          ::CreateConfirmInfoBar(std::move(infobar_delegate)))) {
    raw_delegate->set_was_shown();
  }
}

void AutofillClientIOS::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  callback.Run(ios::GetChromeBrowserProvider()->GetRiskData());
}

bool AutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void AutofillClientIOS::ScanCreditCard(const CreditCardScanCallback& callback) {
  NOTREACHED();
}

void AutofillClientIOS::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:suggestions popupDelegate:delegate];
}

void AutofillClientIOS::HideAutofillPopup() {
  [bridge_ hideAutofillPopup];
}

bool AutofillClientIOS::IsAutocompleteEnabled() {
  // For browser, Autocomplete is always enabled as part of Autofill.
  return GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
}

void AutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

void AutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  if (password_generation_manager_) {
    password_generation_manager_->DetectFormsEligibleForGeneration(forms);
  }
}

void AutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {
}

scoped_refptr<AutofillWebDataService> AutofillClientIOS::GetDatabase() {
  return ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
      browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
}

bool AutofillClientIOS::IsContextSecure() {
  // This implementation differs slightly from other platforms. Other platforms'
  // implementations check for the presence of active mixed content, but because
  // the iOS web view blocks active mixed content without an option to run it,
  // there is no need to check for active mixed conent here.
  web::NavigationManager* manager = web_state_->GetNavigationManager();
  web::NavigationItem* nav_item = manager->GetLastCommittedItem();
  if (!nav_item)
    return false;

  const web::SSLStatus& ssl = nav_item->GetSSL();
  return nav_item->GetURL().SchemeIsCryptographic() && ssl.certificate &&
         (!net::IsCertStatusError(ssl.cert_status) ||
          net::IsCertStatusMinorError(ssl.cert_status));
}

bool AutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

void AutofillClientIOS::StartSigninFlow() {
  NOTIMPLEMENTED();
}

void AutofillClientIOS::ShowHttpNotSecureExplanation() {
  NOTIMPLEMENTED();
}

}  // namespace autofill
