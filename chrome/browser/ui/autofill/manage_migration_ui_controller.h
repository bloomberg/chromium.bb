// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MANAGE_MIGRATION_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MANAGE_MIGRATION_UI_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/autofill/local_card_migration_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog_controller_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Possible steps the migration flow could be in.
enum class LocalCardMigrationFlowStep {
  // Migration flow step unknown.
  UNKNOWN,
  // No migration flow bubble or dialog is shown.
  NOT_SHOWN,
  // Should show the bubble that offers users to continue with the migration
  // flow.
  PROMO_BUBBLE,
  // Should show the dialog that offers users to migrate credit cards to
  // Payments server.
  OFFER_DIALOG,
  // Should show the credit card icon when migration is finished and the
  // feedback dialog is ready.
  CREDIT_CARD_ICON,
  // Should show the feedback dialog dialog after the user clicking the
  // credit card.
  FEEDBACK_DIALOG,
};

// Controller controls the step of migration flow and is responsible
// for interacting with LocalCardMigrationIconView.
class ManageMigrationUiController
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ManageMigrationUiController> {
 public:
  ~ManageMigrationUiController() override;

  void ShowBubble(base::OnceClosure show_migration_dialog_closure);

  void ShowOfferDialog(
      std::unique_ptr<base::DictionaryValue> legal_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      AutofillClient::LocalCardMigrationCallback
          start_migrating_cards_callback);

  void ShowCreditCardIcon(
      const base::string16& tip_message,
      const std::vector<MigratableCreditCard>& migratable_credit_cards);

  void OnUserClickingCreditCardIcon();

  LocalCardMigrationFlowStep GetFlowStep() const;

  bool IsIconVisible() const;

  LocalCardMigrationBubble* GetBubbleView() const;

  LocalCardMigrationDialog* GetDialogView() const;

 protected:
  explicit ManageMigrationUiController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ManageMigrationUiController>;

  void ReshowBubble();

  void ShowFeedbackDialog();

  LocalCardMigrationBubbleControllerImpl* bubble_controller_ = nullptr;
  LocalCardMigrationDialogControllerImpl* dialog_controller_ = nullptr;

  // This indicates what step the migration flow is currently in and
  // what should be shown next.
  LocalCardMigrationFlowStep flow_step_ = LocalCardMigrationFlowStep::NOT_SHOWN;

  DISALLOW_COPY_AND_ASSIGN(ManageMigrationUiController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MANAGE_MIGRATION_UI_CONTROLLER_H_
