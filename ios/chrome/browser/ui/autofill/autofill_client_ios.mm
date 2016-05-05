// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
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
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/web_data_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace autofill {

AutofillClientIOS::AutofillClientIOS(
    ios::ChromeBrowserState* browser_state,
    infobars::InfoBarManager* infobar_manager,
    id<AutofillClientIOSBridge> bridge,
    password_manager::PasswordGenerationManager* password_generation_manager,
    std::unique_ptr<IdentityProvider> identity_provider)
    : browser_state_(browser_state),
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
  return PersonalDataManagerFactory::GetForBrowserState(
      browser_state_->GetOriginalChromeBrowserState());
}

PrefService* AutofillClientIOS::GetPrefs() {
  return browser_state_->GetPrefs();
}

// TODO(jdonnelly): Implement this when adding credit card upload.
sync_driver::SyncService* AutofillClientIOS::GetSyncService() {
  NOTIMPLEMENTED();
  return nullptr;
}

IdentityProvider* AutofillClientIOS::GetIdentityProvider() {
  return identity_provider_.get();
}

// TODO(dconnelly): [Merge] Does this need a real implementation?
// http://crbug.com/468326
rappor::RapporService* AutofillClientIOS::GetRapporService() {
  NOTIMPLEMENTED();
  return nullptr;
}

void AutofillClientIOS::ShowAutofillSettings() {
  NOTREACHED();
}

void AutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  unmask_controller_.ShowPrompt(
      provider->CreateCardUnmaskPromptView(&unmask_controller_), card, reason,
      delegate);
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
      base::WrapUnique(new AutofillSaveCardInfoBarDelegateMobile(
          false, card, std::unique_ptr<base::DictionaryValue>(nullptr),
          callback))));
}

void AutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    const base::Closure& callback) {
  infobar_manager_->AddInfoBar(CreateSaveCardInfoBarMobile(
      base::WrapUnique(new AutofillSaveCardInfoBarDelegateMobile(
          true, card, std::move(legal_message), callback))));
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

bool AutofillClientIOS::IsContextSecure(const GURL& form_origin) {
  // TODO (sigbjorn): Return if the context is secure, not just
  // the form_origin. See crbug.com/505388.
  return form_origin.SchemeIsCryptographic();
}

void AutofillClientIOS::OnFirstUserGestureObserved() {
  // TODO(gcasto): [Merge 306796] http://crbug.com/439425 Verify if this method
  // needs a real implementation or not.
  NOTIMPLEMENTED();
}

}  // namespace autofill
