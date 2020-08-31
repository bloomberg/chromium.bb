// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/generation_confirmation_bubble_controller.h"

#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "ui/base/l10n/l10n_util.h"

namespace metrics_util = password_manager::metrics_util;

GenerationConfirmationBubbleController::GenerationConfirmationBubbleController(
    base::WeakPtr<PasswordsModelDelegate> delegate,
    DisplayReason display_reason)
    : PasswordBubbleControllerBase(
          std::move(delegate),
          /*display_disposition=*/display_reason == DisplayReason::kAutomatic
              ? metrics_util::AUTOMATIC_GENERATED_PASSWORD_CONFIRMATION
              : metrics_util::MANUAL_GENERATED_PASSWORD_CONFIRMATION),
      dismissal_reason_(metrics_util::NO_DIRECT_INTERACTION) {
  base::string16 link = l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_LINK);
  size_t offset = 0;
  save_confirmation_text_ = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_CONFIRM_GENERATED_TEXT, link, &offset);
  save_confirmation_link_range_ = gfx::Range(offset, offset + link.length());
}

GenerationConfirmationBubbleController::
    ~GenerationConfirmationBubbleController() {
  if (!interaction_reported_)
    OnBubbleClosing();
}

void GenerationConfirmationBubbleController::
    OnNavigateToPasswordManagerAccountDashboardLinkClicked(
        password_manager::ManagePasswordsReferrer referrer) {
  dismissal_reason_ = metrics_util::CLICKED_PASSWORDS_DASHBOARD;
  if (delegate_)
    delegate_->NavigateToPasswordManagerAccountDashboard(referrer);
}

base::string16 GenerationConfirmationBubbleController::GetTitle() const {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CONFIRM_SAVED_TITLE);
}

void GenerationConfirmationBubbleController::ReportInteractions() {
  metrics_util::LogGeneralUIDismissalReason(dismissal_reason_);
  // Record UKM statistics on dismissal reason.
  if (metrics_recorder_)
    metrics_recorder_->RecordUIDismissalReason(dismissal_reason_);
}
