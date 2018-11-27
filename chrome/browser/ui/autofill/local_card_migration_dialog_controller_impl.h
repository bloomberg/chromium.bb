// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/ui/local_card_migration_dialog_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

class LocalCardMigrationDialog;

// This per-tab controller is lazily initialized and owns a
// LocalCardMigrationDialog. It's also responsible for reshowing the original
// dialog that the migration dialog interrupted.
class LocalCardMigrationDialogControllerImpl
    : public LocalCardMigrationDialogController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          LocalCardMigrationDialogControllerImpl> {
 public:
  ~LocalCardMigrationDialogControllerImpl() override;

  void ShowOfferDialog(
      std::unique_ptr<base::DictionaryValue> legal_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      AutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback);

  // When migration is finished, show a credit card icon in the omnibox. Also
  // passes |tip_message|, and |migratable_credit_cards| to controller.
  void ShowCreditCardIcon(
      const base::string16& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards);

  // If the user clicks on the credit card icon in the omnibox, we show the
  // feedback dialog containing the uploading results of the cards that the
  // user selected to upload.
  void ShowFeedbackDialog();

  // If the user clicks on the credit card icon in the omnibox after the
  // migration request failed due to some internal server errors, we show the
  // error dialog containing an error message.
  void ShowErrorDialog();

  // LocalCardMigrationDialogController:
  LocalCardMigrationDialogState GetViewState() const override;
  const std::vector<MigratableCreditCard>& GetCardList() const override;
  const LegalMessageLines& GetLegalMessageLines() const override;
  const base::string16& GetTipMessage() const override;
  void OnSaveButtonClicked(
      const std::vector<std::string>& selected_cards_guids) override;
  void OnCancelButtonClicked() override;
  void OnViewCardsButtonClicked() override;
  void OnLegalMessageLinkClicked(const GURL& url) override;
  void OnDialogClosed() override;

  // Returns nullptr if no dialog is currently shown.
  LocalCardMigrationDialog* local_card_migration_dialog_view() const;

 protected:
  explicit LocalCardMigrationDialogControllerImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      LocalCardMigrationDialogControllerImpl>;

  void OpenUrl(const GURL& url);

  void UpdateIcon();

  LocalCardMigrationDialog* local_card_migration_dialog_ = nullptr;

  PrefService* pref_service_;

  LocalCardMigrationDialogState view_state_;

  LegalMessageLines legal_message_lines_;

  // Invoked when the save button is clicked. Will return a vector containing
  // GUIDs of cards that the user selected to upload.
  AutofillClient::LocalCardMigrationCallback start_migrating_cards_callback_;

  // Local copy of the MigratableCreditCards vector passed from
  // LocalCardMigrationManager. Used in constructing the
  // LocalCardMigrationDialogView.
  std::vector<MigratableCreditCard> migratable_credit_cards_;

  // Timer used to measure the amount of time that the local card migration
  // dialog is visible to users.
  base::ElapsedTimer dialog_is_visible_duration_timer_;

  // The message containing information from Google Payments. Shown in the
  // feedback dialogs after migration process is finished.
  base::string16 tip_message_;

  DISALLOW_COPY_AND_ASSIGN(LocalCardMigrationDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOCAL_CARD_MIGRATION_DIALOG_CONTROLLER_IMPL_H_
