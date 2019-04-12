// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/local_card_migration_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/manage_migration_ui_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/local_card_migration_dialog_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

LocalCardMigrationIconView::LocalCardMigrationIconView(
    CommandUpdater* command_updater,
    Browser* browser,
    PageActionIconView::Delegate* delegate,
    const gfx::FontList& font_list)
    : PageActionIconView(command_updater,
                         IDC_MIGRATE_LOCAL_CREDIT_CARD_FOR_PAGE,
                         delegate,
                         font_list),
      browser_(browser) {
  DCHECK(delegate);
  set_id(VIEW_ID_MIGRATE_LOCAL_CREDIT_CARD_BUTTON);
  SetUpForInOutAnimation();
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

  // |controller| may be nullptr due to lazy initialization.
  ManageMigrationUiController* controller = GetController();
  bool enabled = controller && controller->IsIconVisible();
  enabled &= SetCommandEnabled(enabled);
  SetVisible(enabled);

  if (visible()) {
    switch (controller->GetFlowStep()) {
      // When the dialog is about to show, trigger the ink drop animation
      // so that the credit card icon in "selected" state by default. This needs
      // to be manually set since the migration dialog is not anchored at the
      // credit card icon.
      case LocalCardMigrationFlowStep::OFFER_DIALOG: {
        UpdateIconImage();
        AnimateInkDrop(views::InkDropState::ACTIVATED, /*event=*/nullptr);
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING: {
        AnimateInkDrop(views::InkDropState::HIDDEN, /*event=*/nullptr);
        // Disable the credit card icon so it does not update if user clicks
        // on it.
        SetEnabled(false);
        AnimateIn(IDS_AUTOFILL_LOCAL_CARD_MIGRATION_ANIMATION_LABEL);
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_FINISHED: {
        UnpauseAnimation();
        SetEnabled(true);
        break;
      }
      case LocalCardMigrationFlowStep::MIGRATION_FAILED: {
        UnpauseAnimation();
        SetEnabled(true);
        break;
      }
      default:
        break;
    }
  } else {
    // Handle corner cases where users navigate away or close the tab.
    UnpauseAnimation();
  }

  // Need to return true since in both MIGRATION_RESULT_PENDING and
  // MIGRATION_FINISHED cases the credit card icon is visible.
  return true;
}

void LocalCardMigrationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& LocalCardMigrationIconView::GetVectorIcon() const {
  return kCreditCardIcon;
}

const gfx::VectorIcon& LocalCardMigrationIconView::GetVectorIconBadge() const {
  ManageMigrationUiController* controller = GetController();
  if (controller && controller->GetFlowStep() ==
                        LocalCardMigrationFlowStep::MIGRATION_FAILED) {
    return kBlockedBadgeIcon;
  }
  return gfx::kNoneIcon;
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

void LocalCardMigrationIconView::AnimationProgressed(
    const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationProgressed(animation);

  // Pause the animation when the animation text is completely shown. If the
  // user navigates to other tab when the animation is in progress, don't pause
  // the animation.
  constexpr double animation_text_full_length_shown_state = 0.5;
  if (GetController() &&
      GetController()->GetFlowStep() ==
          LocalCardMigrationFlowStep::MIGRATION_RESULT_PENDING &&
      GetAnimationValue() >= animation_text_full_length_shown_state) {
    PauseAnimation();
  }
}

void LocalCardMigrationIconView::AnimationEnded(
    const gfx::Animation* animation) {
  IconLabelBubbleView::AnimationEnded(animation);
  UpdateIconImage();
}

}  // namespace autofill
