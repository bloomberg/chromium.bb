// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/manage_migration_ui_controller.h"

#include "chrome/browser/ui/autofill/local_card_migration_bubble.h"
#include "chrome/browser/ui/autofill/local_card_migration_dialog.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"

namespace autofill {

ManageMigrationUiController::ManageMigrationUiController(
    content::WebContents* web_contents) {
  autofill::LocalCardMigrationBubbleControllerImpl::CreateForWebContents(
      web_contents);
  bubble_controller_ =
      autofill::LocalCardMigrationBubbleControllerImpl::FromWebContents(
          web_contents);
  bubble_controller_->AddObserver(this);
  autofill::LocalCardMigrationDialogControllerImpl::CreateForWebContents(
      web_contents);
  dialog_controller_ =
      autofill::LocalCardMigrationDialogControllerImpl::FromWebContents(
          web_contents);
}

ManageMigrationUiController::~ManageMigrationUiController() {}

void ManageMigrationUiController::ShowBubble(
    base::OnceClosure show_migration_dialog_closure) {
  flow_step_ = LocalCardMigrationFlowStep::PROMO_BUBBLE;
  bubble_controller_->ShowBubble(std::move(show_migration_dialog_closure));
}

void ManageMigrationUiController::ShowOfferDialog(
    std::unique_ptr<base::DictionaryValue> legal_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    AutofillClient::LocalCardMigrationCallback start_migrating_cards_callback) {
  flow_step_ = LocalCardMigrationFlowStep::OFFER_DIALOG;
  dialog_controller_->ShowOfferDialog(
      std::move(legal_message), migratable_credit_cards,
      std::move(start_migrating_cards_callback));
}

void ManageMigrationUiController::ShowCreditCardIcon(
    const base::string16& tip_message,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    AutofillClient::MigrationDeleteCardCallback delete_local_card_callback) {
  if (!dialog_controller_)
    return;

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::OFFER_DIALOG);
  flow_step_ = LocalCardMigrationFlowStep::CREDIT_CARD_ICON;
  // Show error dialog when the vector is an empty vector, which indicates
  // Payments Rpc failure.
  show_error_dialog_ = migratable_credit_cards.empty();
  dialog_controller_->ShowCreditCardIcon(tip_message, migratable_credit_cards,
                                         delete_local_card_callback);
}

void ManageMigrationUiController::OnUserClickedCreditCardIcon() {
  switch (flow_step_) {
    case LocalCardMigrationFlowStep::PROMO_BUBBLE: {
      ReshowBubble();
      break;
    }
    case LocalCardMigrationFlowStep::CREDIT_CARD_ICON: {
      show_error_dialog_ ? ShowErrorDialog() : ShowFeedbackDialog();
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

LocalCardMigrationFlowStep ManageMigrationUiController::GetFlowStep() const {
  return flow_step_;
}

bool ManageMigrationUiController::IsIconVisible() const {
  return flow_step_ == LocalCardMigrationFlowStep::PROMO_BUBBLE ||
         flow_step_ == LocalCardMigrationFlowStep::CREDIT_CARD_ICON;
}

LocalCardMigrationBubble* ManageMigrationUiController::GetBubbleView() const {
  if (!bubble_controller_)
    return nullptr;

  return bubble_controller_->local_card_migration_bubble_view();
}

LocalCardMigrationDialog* ManageMigrationUiController::GetDialogView() const {
  if (!dialog_controller_)
    return nullptr;

  return dialog_controller_->local_card_migration_dialog_view();
}

void ManageMigrationUiController::OnMigrationNoLongerAvailable() {
  flow_step_ = LocalCardMigrationFlowStep::NOT_SHOWN;
}

void ManageMigrationUiController::ReshowBubble() {
  if (!bubble_controller_)
    return;

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::PROMO_BUBBLE);
  bubble_controller_->ReshowBubble();
}

void ManageMigrationUiController::ShowErrorDialog() {
  if (!dialog_controller_)
    return;

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::CREDIT_CARD_ICON);
  flow_step_ = LocalCardMigrationFlowStep::ERROR_DIALOG;
  dialog_controller_->ShowErrorDialog();
}

void ManageMigrationUiController::ShowFeedbackDialog() {
  if (!dialog_controller_)
    return;

  DCHECK_EQ(flow_step_, LocalCardMigrationFlowStep::CREDIT_CARD_ICON);
  flow_step_ = LocalCardMigrationFlowStep::FEEDBACK_DIALOG;
  dialog_controller_->ShowFeedbackDialog();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ManageMigrationUiController)

}  // namespace autofill
