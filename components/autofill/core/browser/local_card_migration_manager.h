// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOCAL_CARD_MIGRATION_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOCAL_CARD_MIGRATION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {

class CreditCard;
class PersonalDataManager;

// MigratableCreditCard class is used as a DataStructure to work as an
// intermediary between the UI side and the migration manager. Besides the basic
// credit card information, it also includes a boolean that represents whether
// the card was chosen for upload.
// TODO(crbug.com/852904): Create one Enum to represent migration status such as
// whether the card is successfully uploaded or failure on uploading.
class MigratableCreditCard {
 public:
  MigratableCreditCard(const CreditCard& credit_card);
  ~MigratableCreditCard();

  CreditCard credit_card() const { return credit_card_; }

  bool is_chosen() const { return is_chosen_; }
  void set_is_chosen(bool is_chosen) { is_chosen_ = is_chosen; }

 private:
  // The main card information of the current migratable card.
  CreditCard credit_card_;

  // Whether the user has decided to migrate the this card; shown as a checkbox
  // in the UI.
  bool is_chosen_ = true;
};

// Manages logic for determining whether migration of locally saved credit cards
// to Google Payments is available as well as multiple local card uploading.
// Owned by FormDataImporter.
class LocalCardMigrationManager {
 public:
  // The parameters should outlive the LocalCardMigrationManager.
  LocalCardMigrationManager(AutofillClient* client,
                            payments::PaymentsClient* payments_client,
                            const std::string& app_locale,
                            PersonalDataManager* personal_data_manager);
  virtual ~LocalCardMigrationManager();

  // Returns true if all of the conditions for allowing local credit card
  // migration are satisfied. Initializes the local card list for upload.
  bool ShouldOfferLocalCardMigration(int imported_credit_card_record_type);

  // Called from FormDataImporter when all migration requirements are met.
  // Fetches legal documents and triggers the OnDidGetUploadDetails callback.
  void AttemptToOfferLocalCardMigration();

  // Callback function when user agrees to migration on the intermediate dialog.
  // Pops up a larger, modal dialog showing the local cards to be uploaded.
  void OnUserAcceptedIntermediateMigrationDialog();

  // Check that the user is signed in, syncing, and the proper experiment
  // flags are enabled. Override in the test class.
  virtual bool IsCreditCardMigrationEnabled();

  // Determines what detected_values metadata to send (generally, cardholder
  // name if it exists on all cards, and existence of Payments customer).
  int GetDetectedValues() const;

 protected:
  // Callback after successfully getting the legal documents. On success,
  // displays the offer-to-migrate dialog, which the user can accept or not.
  // Exposed for testing.
  virtual void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::DictionaryValue> legal_message);

  // Fetch all migratable credit cards and store in |migratable_credit_cards_|.
  void GetMigratableCreditCards();

  AutofillClient* const client_;

  // Handles Payments service requests.
  // Owned by AutofillManager.
  payments::PaymentsClient* payments_client_;

 private:
  // Callback function when user confirms migration on the main migration
  // dialog. Sets |user_accepted_main_migration_dialog_| and sends the upload
  // request.
  void OnUserAcceptedMainMigrationDialog();

  std::unique_ptr<base::DictionaryValue> legal_message_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_manager_;

  // Collected information about a pending upload request.
  payments::PaymentsClient::UploadRequestDetails upload_request_;

  // The local credit cards to be uploaded.
  std::vector<MigratableCreditCard> migratable_credit_cards_;

  // |true| if the user has accepted migrating their local cards to Google Pay
  // on the main dialog.
  bool user_accepted_main_migration_dialog_ = false;

  base::WeakPtrFactory<LocalCardMigrationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOCAL_CARD_MIGRATION_MANAGER_H_
