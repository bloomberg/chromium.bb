// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/chrome_autofill_client_ios.h"

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
    : AutofillClientIOS(
          browser_state->GetPrefs(),
          PersonalDataManagerFactory::GetForBrowserState(browser_state),
          web_state,
          bridge,
          std::move(identity_provider),
          ios::WebDataServiceFactory::GetAutofillWebDataForBrowserState(
              browser_state,
              ServiceAccessType::EXPLICIT_ACCESS)),
      infobar_manager_(infobar_manager),
      password_generation_manager_(password_generation_manager),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

ChromeAutofillClientIOS::~ChromeAutofillClientIOS() {}

ukm::UkmRecorder* ChromeAutofillClientIOS::GetUkmRecorder() {
  return GetApplicationContext()->GetUkmRecorder();
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
      base::MakeUnique<AutofillSaveCardInfoBarDelegateMobile>(
          false, card, std::unique_ptr<base::DictionaryValue>(nullptr),
          callback, GetPrefs())));
}

void ChromeAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_cvc_be_requested,
    const base::Closure& callback) {
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      base::MakeUnique<AutofillSaveCardInfoBarDelegateMobile>(
          true, card, std::move(legal_message), callback, GetPrefs())));
}

void ChromeAutofillClientIOS::ConfirmCreditCardFillAssist(
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

void ChromeAutofillClientIOS::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {
  callback.Run(ios::GetChromeBrowserProvider()->GetRiskData());
}

void ChromeAutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  if (password_generation_manager_) {
    password_generation_manager_->DetectFormsEligibleForGeneration(forms);
  }
}

}  // namespace autofill
