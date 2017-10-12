// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_view.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/identity_provider.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

AutofillClientIOS::AutofillClientIOS(
    PrefService* pref_service,
    PersonalDataManager* personal_data_manager,
    web::WebState* web_state,
    id<AutofillClientIOSBridge> bridge,
    std::unique_ptr<IdentityProvider> identity_provider,
    scoped_refptr<AutofillWebDataService> autofill_web_data_service)
    : pref_service_(pref_service),
      personal_data_manager_(personal_data_manager),
      web_state_(web_state),
      bridge_(bridge),
      identity_provider_(std::move(identity_provider)),
      autofill_web_data_service_(autofill_web_data_service) {}

AutofillClientIOS::~AutofillClientIOS() {
  HideAutofillPopup();
}

PersonalDataManager* AutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

PrefService* AutofillClientIOS::GetPrefs() {
  return pref_service_;
}

// TODO(crbug.com/535784): Implement this when adding credit card upload.
syncer::SyncService* AutofillClientIOS::GetSyncService() {
  return nullptr;
}

IdentityProvider* AutofillClientIOS::GetIdentityProvider() {
  return identity_provider_.get();
}

ukm::UkmRecorder* AutofillClientIOS::GetUkmRecorder() {
  return nullptr;
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
    base::WeakPtr<CardUnmaskDelegate> delegate) {}

void AutofillClientIOS::OnUnmaskVerificationResult(PaymentsRpcResult result) {}

void AutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    const base::Closure& callback) {}

void AutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_cvc_be_requested,
    const base::Closure& callback) {}

void AutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    const base::Closure& callback) {}

void AutofillClientIOS::LoadRiskData(
    const base::Callback<void(const std::string&)>& callback) {}

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
    const std::vector<FormStructure*>& forms) {}

void AutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

scoped_refptr<AutofillWebDataService> AutofillClientIOS::GetDatabase() {
  return autofill_web_data_service_;
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

void AutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

bool AutofillClientIOS::IsAutofillSupported() {
  return true;
}

}  // namespace autofill
