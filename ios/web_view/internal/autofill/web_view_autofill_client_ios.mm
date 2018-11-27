// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/ios/browser/autofill_util.h"
#include "ios/web_view/internal/app/application_context.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {

WebViewAutofillClientIOS::WebViewAutofillClientIOS(
    PrefService* pref_service,
    PersonalDataManager* personal_data_manager,
    web::WebState* web_state,
    id<CWVAutofillClientIOSBridge> bridge,
    identity::IdentityManager* identity_manager,
    LegacyStrikeDatabase* strike_database,
    scoped_refptr<AutofillWebDataService> autofill_web_data_service,
    syncer::SyncService* sync_service)
    : pref_service_(pref_service),
      personal_data_manager_(personal_data_manager),
      web_state_(web_state),
      bridge_(bridge),
      identity_manager_(identity_manager),
      payments_client_(std::make_unique<payments::PaymentsClient>(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              web_state_->GetBrowserState()->GetURLLoaderFactory()),
          pref_service_,
          identity_manager_,
          personal_data_manager_,
          web_state_->GetBrowserState()->IsOffTheRecord())),
      form_data_importer_(std::make_unique<FormDataImporter>(
          this,
          payments_client_.get(),
          personal_data_manager_,
          ios_web_view::ApplicationContext::GetInstance()
              ->GetApplicationLocale())),
      legacy_strike_database_(strike_database),
      autofill_web_data_service_(autofill_web_data_service),
      sync_service_(sync_service) {}

WebViewAutofillClientIOS::~WebViewAutofillClientIOS() {
  HideAutofillPopup();
}

PersonalDataManager* WebViewAutofillClientIOS::GetPersonalDataManager() {
  return personal_data_manager_;
}

PrefService* WebViewAutofillClientIOS::GetPrefs() {
  return pref_service_;
}

syncer::SyncService* WebViewAutofillClientIOS::GetSyncService() {
  return sync_service_;
}

identity::IdentityManager* WebViewAutofillClientIOS::GetIdentityManager() {
  return identity_manager_;
}

FormDataImporter* WebViewAutofillClientIOS::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* WebViewAutofillClientIOS::GetPaymentsClient() {
  return payments_client_.get();
}

LegacyStrikeDatabase* WebViewAutofillClientIOS::GetLegacyStrikeDatabase() {
  return legacy_strike_database_;
}

ukm::UkmRecorder* WebViewAutofillClientIOS::GetUkmRecorder() {
  // UKM recording is not supported for WebViews.
  return nullptr;
}

ukm::SourceId WebViewAutofillClientIOS::GetUkmSourceId() {
  // UKM recording is not supported for WebViews.
  return 0;
}

AddressNormalizer* WebViewAutofillClientIOS::GetAddressNormalizer() {
  return nullptr;
}

security_state::SecurityLevel
WebViewAutofillClientIOS::GetSecurityLevelForUmaHistograms() {
  // The metrics are not recorded for iOS webview, so return the count value
  // which will not be recorded.
  return security_state::SecurityLevel::SECURITY_LEVEL_COUNT;
}

void WebViewAutofillClientIOS::ShowAutofillSettings(
    bool show_credit_card_settings) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
  [bridge_ showUnmaskPromptForCard:card reason:reason delegate:delegate];
}

void WebViewAutofillClientIOS::OnUnmaskVerificationResult(
    PaymentsRpcResult result) {
  [bridge_ didReceiveUnmaskVerificationResult:result];
}

void WebViewAutofillClientIOS::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::ConfirmMigrateLocalCardToCloud(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::ShowLocalCardMigrationResults(
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards) {
  NOTIMPLEMENTED();
}

void WebViewAutofillClientIOS::ConfirmSaveAutofillProfile(
    const AutofillProfile& profile,
    base::OnceClosure callback) {
  // Since there is no confirmation needed to save an Autofill Profile,
  // running |callback| will proceed with saving |profile|.
  std::move(callback).Run();
}

void WebViewAutofillClientIOS::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    bool show_prompt,
    base::OnceClosure callback) {
  DCHECK(show_prompt);
  [bridge_ confirmSaveCreditCardLocally:card callback:std::move(callback)];
}

void WebViewAutofillClientIOS::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    bool should_request_expiration_date_from_user,
    bool show_prompt,
    UserAcceptedUploadCallback callback) {
  DCHECK(show_prompt);
}

void WebViewAutofillClientIOS::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {}

void WebViewAutofillClientIOS::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  [bridge_ loadRiskData:std::move(callback)];
}

bool WebViewAutofillClientIOS::HasCreditCardScanFeature() {
  return false;
}

void WebViewAutofillClientIOS::ScanCreditCard(
    const CreditCardScanCallback& callback) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::ShowAutofillPopup(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const std::vector<Suggestion>& suggestions,
    bool /*unused_autoselect_first_suggestion*/,
    base::WeakPtr<AutofillPopupDelegate> delegate) {
  [bridge_ showAutofillPopup:suggestions popupDelegate:delegate];
}

void WebViewAutofillClientIOS::HideAutofillPopup() {
  [bridge_ hideAutofillPopup];
}

bool WebViewAutofillClientIOS::IsAutocompleteEnabled() {
  return prefs::IsAutocompleteEnabled(GetPrefs());
}

void WebViewAutofillClientIOS::UpdateAutofillPopupDataListValues(
    const std::vector<base::string16>& values,
    const std::vector<base::string16>& labels) {
  NOTREACHED();
}

void WebViewAutofillClientIOS::PropagateAutofillPredictions(
    content::RenderFrameHost* rfh,
    const std::vector<FormStructure*>& forms) {
  [bridge_ propagateAutofillPredictionsForForms:forms];
}

void WebViewAutofillClientIOS::DidFillOrPreviewField(
    const base::string16& autofilled_value,
    const base::string16& profile_full_name) {}

scoped_refptr<AutofillWebDataService> WebViewAutofillClientIOS::GetDatabase() {
  return autofill_web_data_service_;
}

void WebViewAutofillClientIOS::DidInteractWithNonsecureCreditCardInput() {}

bool WebViewAutofillClientIOS::IsContextSecure() {
  return IsContextSecureForWebState(web_state_);
}

bool WebViewAutofillClientIOS::ShouldShowSigninPromo() {
  return false;
}

void WebViewAutofillClientIOS::ExecuteCommand(int id) {
  NOTIMPLEMENTED();
}

bool WebViewAutofillClientIOS::AreServerCardsSupported() {
  return true;
}

}  // namespace autofill
