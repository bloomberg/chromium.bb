// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/local_card_migration_manager.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace autofill {

MigratableCreditCard::MigratableCreditCard(const CreditCard& credit_card)
    : credit_card_(credit_card) {}

MigratableCreditCard::~MigratableCreditCard() {}

LocalCardMigrationManager::LocalCardMigrationManager(
    AutofillClient* client,
    payments::PaymentsClient* payments_client,
    const std::string& app_locale,
    PersonalDataManager* personal_data_manager)
    : client_(client),
      payments_client_(payments_client),
      app_locale_(app_locale),
      personal_data_manager_(personal_data_manager),
      weak_ptr_factory_(this) {}

LocalCardMigrationManager::~LocalCardMigrationManager() {}

bool LocalCardMigrationManager::ShouldOfferLocalCardMigration(
    int imported_credit_card_record_type) {
  // Must be an existing card. New cards always get Upstream or local save.
  if (imported_credit_card_record_type !=
          FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD &&
      imported_credit_card_record_type !=
          FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD) {
    return false;
  }

  if (!IsCreditCardMigrationEnabled())
    return false;

  // Fetch all migratable credit cards and store in |migratable_credit_cards_|.
  GetMigratableCreditCards();

  // If the form was submitted with a local card, only offer migration instead
  // of Upstream if there are other local cards to migrate as well. If the form
  // was submitted with a server card, offer migration if ANY local cards can be
  // migrated.
  return (imported_credit_card_record_type ==
              FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD &&
          migratable_credit_cards_.size() > 1) ||
         (imported_credit_card_record_type ==
              FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD &&
          !migratable_credit_cards_.empty());
}

void LocalCardMigrationManager::AttemptToOfferLocalCardMigration(
    bool is_from_settings_page) {
  // Abort the migration if |payments_client_| is nullptr.
  if (!payments_client_)
    return;
  upload_request_ = payments::PaymentsClient::UploadRequestDetails();

  // Payments server determines which version of the legal message to show based
  // on the existence of this experiment flag.
  if (features::IsAutofillUpstreamUpdatePromptExplanationExperimentEnabled()) {
    upload_request_.active_experiments.push_back(
        features::kAutofillUpstreamUpdatePromptExplanation.name);
  }

  // Don't send pan_first_six, as potentially migrating multiple local cards at
  // once will negate its usefulness.
  payments_client_->GetUploadDetails(
      upload_request_.profiles, GetDetectedValues(),
      /*pan_first_six=*/std::string(), upload_request_.active_experiments,
      app_locale_,
      base::BindOnce(&LocalCardMigrationManager::OnDidGetUploadDetails,
                     weak_ptr_factory_.GetWeakPtr(), is_from_settings_page),
      payments::kMigrateCardsBillableServiceNumber);
}

// Callback function when user agrees to migration on the intermediate dialog.
// Call ShowMainMigrationDialog() to pop up a larger, modal dialog showing the
// local cards to be uploaded.
void LocalCardMigrationManager::OnUserAcceptedIntermediateMigrationDialog() {
  ShowMainMigrationDialog();
}

// Send the migration request once risk data is available.
void LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog() {
  user_accepted_main_migration_dialog_ = true;
  // Populating risk data and offering migration two-round pop-ups occur
  // asynchronously. If |migration_risk_data_| has already been loaded, send the
  // migrate local cards request. Otherwise, continue to wait and let
  // OnDidGetUploadRiskData handle it.
  if (!migration_risk_data_.empty())
    SendMigrateLocalCardsRequest();
}

bool LocalCardMigrationManager::IsCreditCardMigrationEnabled() {
  // Confirm that the user is signed in, syncing, and the proper experiment
  // flags are enabled.
  bool migration_experiment_enabled =
      features::GetLocalCardMigrationExperimentalFlag() !=
      features::LocalCardMigrationExperimentalFlag::kMigrationDisabled;
  bool credit_card_upload_enabled = ::autofill::IsCreditCardUploadEnabled(
      client_->GetPrefs(), client_->GetSyncService(),
      client_->GetIdentityManager()->GetPrimaryAccountInfo().email);
  bool has_google_payments_account =
      (payments::GetBillingCustomerId(personal_data_manager_,
                                      payments_client_->GetPrefService()) != 0);
  return migration_experiment_enabled && credit_card_upload_enabled &&
         has_google_payments_account;
}

void LocalCardMigrationManager::OnDidGetUploadDetails(
    bool is_from_settings_page,
    AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  if (result == AutofillClient::SUCCESS) {
    upload_request_.context_token = context_token;
    legal_message_ = std::move(legal_message);
    migration_risk_data_.clear();
    // If we successfully received the legal docs, trigger the offer-to-migrate
    // dialog. If triggered from settings page, we pop-up the main prompt
    // directly. If not, we pop up the intermediate bubble.
    if (is_from_settings_page) {
      // Pops up a larger, modal dialog showing the local cards to be uploaded.
      ShowMainMigrationDialog();
    } else {
      client_->ShowLocalCardMigrationDialog(base::BindOnce(
          &LocalCardMigrationManager::OnUserAcceptedIntermediateMigrationDialog,
          weak_ptr_factory_.GetWeakPtr()));
    }
    // TODO(crbug.com/876895): Clean up the LoadRiskData Bind/BindRepeating
    // usages
    client_->LoadRiskData(base::BindRepeating(
        &LocalCardMigrationManager::OnDidGetMigrationRiskData,
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void LocalCardMigrationManager::OnDidGetMigrationRiskData(
    const std::string& risk_data) {
  migration_risk_data_ = risk_data;
  // Populating risk data and offering migration two-round pop-ups occur
  // asynchronously. If the main migration dialog has already been accepted,
  // send the migrate local cards request. Otherwise, continue to wait for the
  // user to accept the two round dialog.
  if (user_accepted_main_migration_dialog_)
    SendMigrateLocalCardsRequest();
}

// TODO(crbug.com/852904): Send the migration request. Will call payments_client
// to create a new PaymentsRequest. Also create a new callback function
// OnDidMigrateLocalCards.
void LocalCardMigrationManager::SendMigrateLocalCardsRequest() {}

// Pops up a larger, modal dialog showing the local cards to be uploaded. Pass
// the reference of vector<MigratableCreditCard> and the callback function is
// OnUserAcceptedMainMigrationDialog(). Can be called when user agrees to
// migration on the intermediate dialog or directly from settings page.
void LocalCardMigrationManager::ShowMainMigrationDialog() {
  user_accepted_main_migration_dialog_ = false;
  // Pops up a larger, modal dialog showing the local cards to be uploaded.
  client_->ConfirmMigrateLocalCardToCloud(
      migratable_credit_cards_,
      base::BindOnce(
          &LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog,
          weak_ptr_factory_.GetWeakPtr()));
}

int LocalCardMigrationManager::GetDetectedValues() const {
  int detected_values = 0;

  // If all cards to be migrated have a cardholder name, include it in the
  // detected values.
  bool all_cards_have_cardholder_name = true;
  for (MigratableCreditCard migratable_credit_card : migratable_credit_cards_) {
    all_cards_have_cardholder_name &=
        !migratable_credit_card.credit_card()
             .GetInfo(AutofillType(CREDIT_CARD_NAME_FULL), app_locale_)
             .empty();
  }
  if (all_cards_have_cardholder_name)
    detected_values |= CreditCardSaveManager::DetectedValue::CARDHOLDER_NAME;

  // Local card migration should ONLY be offered when the user already has a
  // Google Payments account.
  DCHECK_NE(0, payments::GetBillingCustomerId(
                   personal_data_manager_, payments_client_->GetPrefService()));
  detected_values |=
      CreditCardSaveManager::DetectedValue::HAS_GOOGLE_PAYMENTS_ACCOUNT;

  return detected_values;
}

void LocalCardMigrationManager::GetMigratableCreditCards() {
  std::vector<CreditCard*> local_credit_cards =
      personal_data_manager_->GetLocalCreditCards();

  // Empty previous state.
  migratable_credit_cards_.clear();

  // Initialize the local credit card list and queue for showing and uploading.
  for (CreditCard* credit_card : local_credit_cards) {
    // If the card is valid (has a valid card number, expiration date, and is
    // not expired) and is not a server card, add it to the list of migratable
    // cards.
    if (credit_card->IsValid() &&
        !personal_data_manager_->IsServerCard(credit_card)) {
      migratable_credit_cards_.push_back(MigratableCreditCard(*credit_card));
    }
  }
}

}  // namespace autofill
