// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_autofill_client.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace autofill {

TestAutofillClient::TestAutofillClient()
    : form_origin_(GURL("https://example.test")), source_id_(-1) {}

TestAutofillClient::~TestAutofillClient() {
}

PersonalDataManager* TestAutofillClient::GetPersonalDataManager() {
  return &test_personal_data_manager_;
}

AutocompleteHistoryManager*
TestAutofillClient::GetAutocompleteHistoryManager() {
  return &mock_autocomplete_history_manager_;
}

PrefService* TestAutofillClient::GetPrefs() {
  return prefs_.get();
}

syncer::SyncService* TestAutofillClient::GetSyncService() {
  return test_sync_service_;
}

identity::IdentityManager* TestAutofillClient::GetIdentityManager() {
  return identity_test_env_.identity_manager();
}

FormDataImporter* TestAutofillClient::GetFormDataImporter() {
  return form_data_importer_.get();
}

payments::PaymentsClient* TestAutofillClient::GetPaymentsClient() {
  return payments_client_.get();
}

LegacyStrikeDatabase* TestAutofillClient::GetLegacyStrikeDatabase() {
  return test_legacy_strike_database_.get();
}

StrikeDatabase* TestAutofillClient::GetStrikeDatabase() {
  return test_strike_database_.get();
}

ukm::UkmRecorder* TestAutofillClient::GetUkmRecorder() {
  return ukm::UkmRecorder::Get();
}

ukm::SourceId TestAutofillClient::GetUkmSourceId() {
  if (source_id_ == -1) {
    source_id_ = ukm::UkmRecorder::GetNewSourceID();
    UpdateSourceURL(GetUkmRecorder(), source_id_, form_origin_);
  }
  return source_id_;
}

AddressNormalizer* TestAutofillClient::GetAddressNormalizer() {
  return &test_address_normalizer_;
}

security_state::SecurityLevel
TestAutofillClient::GetSecurityLevelForUmaHistograms() {
  return security_level_;
}

void TestAutofillClient::ShowAutofillSettings(bool show_credit_card_settings) {}

void TestAutofillClient::ShowUnmaskPrompt(
    const CreditCard& card,
    UnmaskCardReason reason,
    base::WeakPtr<CardUnmaskDelegate> delegate) {
}

void TestAutofillClient::OnUnmaskVerificationResult(PaymentsRpcResult result) {
}

void TestAutofillClient::ShowLocalCardMigrationDialog(
    base::OnceClosure show_migration_dialog_closure) {
  std::move(show_migration_dialog_closure).Run();
}

void TestAutofillClient::ConfirmMigrateLocalCardToCloud(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::string& user_email,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    LocalCardMigrationCallback start_migrating_cards_callback) {
  // If |migration_card_selection_| hasn't been preset by tests, default to
  // selecting all migratable cards.
  if (migration_card_selection_.empty()) {
    for (MigratableCreditCard card : migratable_credit_cards)
      migration_card_selection_.push_back(card.credit_card().guid());
  }
  std::move(start_migrating_cards_callback).Run(migration_card_selection_);
}

void TestAutofillClient::ShowLocalCardMigrationResults(
    const bool has_server_error,
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrationDeleteCardCallback delete_local_card_callback) {}

void TestAutofillClient::ConfirmSaveAutofillProfile(
    const AutofillProfile& profile,
    base::OnceClosure callback) {
  // Since there is no confirmation needed to save an Autofill Profile,
  // running |callback| will proceed with saving |profile|.
  std::move(callback).Run();
}

void TestAutofillClient::ConfirmSaveCreditCardLocally(
    const CreditCard& card,
    bool show_prompt,
    LocalSaveCardPromptCallback callback) {
  confirm_save_credit_card_locally_called_ = true;
  offer_to_save_credit_card_bubble_was_shown_ = show_prompt;
  std::move(callback).Run(AutofillClient::ACCEPTED);
}

#if defined(OS_ANDROID)
void TestAutofillClient::ConfirmAccountNameFixFlow(
    base::OnceCallback<void(const base::string16&)> callback) {
  credit_card_name_fix_flow_bubble_was_shown_ = true;
  std::move(callback).Run(base::string16(base::ASCIIToUTF16("Gaia Name")));
}
#endif  // defined(OS_ANDROID)

void TestAutofillClient::ConfirmSaveCreditCardToCloud(
    const CreditCard& card,
    std::unique_ptr<base::DictionaryValue> legal_message,
    bool should_request_name_from_user,
    bool should_request_expiration_date_from_user,
    bool show_prompt,
    UploadSaveCardPromptCallback callback) {
  offer_to_save_credit_card_bubble_was_shown_ = show_prompt;
  std::move(callback).Run(AutofillClient::ACCEPTED, {});
}

void TestAutofillClient::ConfirmCreditCardFillAssist(
    const CreditCard& card,
    base::OnceClosure callback) {
  std::move(callback).Run();
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
    bool autoselect_first_suggestion,
    base::WeakPtr<AutofillPopupDelegate> delegate) {}

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

bool TestAutofillClient::AreServerCardsSupported() {
  return true;
}

void TestAutofillClient::ExecuteCommand(int id) {}

void TestAutofillClient::LoadRiskData(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run("some risk data");
}

void TestAutofillClient::InitializeUKMSources() {
  UpdateSourceURL(GetUkmRecorder(), source_id_, form_origin_);
}

void TestAutofillClient::set_form_origin(const GURL& url) {
  form_origin_ = url;
  // Also reset source_id_.
  source_id_ = ukm::UkmRecorder::GetNewSourceID();
  UpdateSourceURL(GetUkmRecorder(), source_id_, form_origin_);
}

// static
void TestAutofillClient::UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                                         ukm::SourceId source_id,
                                         GURL url) {
  if (ukm_recorder) {
    ukm_recorder->UpdateSourceURL(source_id, url);
  }
}

}  // namespace autofill
