// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/views/autofill/local_card_migration_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/manage_migration_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/local_card_migration_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

LocalCardMigrationIconView::LocalCardMigrationIconView(
    CommandUpdater* command_updater,
    Browser* browser,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE,
                         delegate),
      browser_(browser) {
  DCHECK(delegate);
  set_id(VIEW_ID_MIGRATE_LOCAL_CREDIT_CARD_BUTTON);
}

LocalCardMigrationIconView::~LocalCardMigrationIconView() {}

views::BubbleDialogDelegateView* LocalCardMigrationIconView::GetBubble() const {
  ManageMigrationUiController* controller = GetController();
  if (!controller)
    return nullptr;

  LocalCardMigrationFlowStep step = controller->GetFlowStep();
  DCHECK_NE(step, LocalCardMigrationFlowStep::UNKNOWN);
  switch (step) {
    case LocalCardMigrationFlowStep::PROMO_BUBBLE:
      return static_cast<LocalCardMigrationBubbleViews*>(
          controller->GetBubbleView());
    case LocalCardMigrationFlowStep::NOT_SHOWN:
    case LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING:
      return nullptr;
    default:
      return static_cast<LocalCardMigrationDialogView*>(
          controller->GetDialogView());
  }
}

bool LocalCardMigrationIconView::Update() {
  if (!GetWebContents())
    return false;

  const bool was_visible = visible();

  // |controller| may be nullptr due to lazy initialization.
  ManageMigrationUiController* controller = GetController();
  bool enabled = controller && controller->IsIconVisible();
  enabled &= SetCommandEnabled(enabled);
  SetVisible(enabled);

  // When the dialog is about to show, trigger the ink drop animation
  // so that the credit card icon in "selected" state by default. This needs
  // to be manually set since the migration dialog is not anchored at the
  // credit card icon.
  if (visible() &&
      controller->GetFlowStep() == LocalCardMigrationFlowStep::OFFER_DIALOG) {
    AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  }

  return was_visible != visible();
}

void LocalCardMigrationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& LocalCardMigrationIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

base::string16 LocalCardMigrationIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_TOOLTIP_MIGRATE_LOCAL_CARD);
}

ManageMigrationUiController* LocalCardMigrationIconView::GetController() const {
  if (!browser_)
    return nullptr;

  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  return autofill::ManageMigrationUiController::FromWebContents(web_contents);
}

}  // namespace autofill
