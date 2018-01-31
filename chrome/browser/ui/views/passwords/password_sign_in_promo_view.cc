// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_sign_in_promo_view.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/fill_layout.h"

PasswordSignInPromoView::PasswordSignInPromoView(
    ManagePasswordsBubbleModel* model)
    : model_(model) {
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_LABEL),
      CONTEXT_BODY_TEXT_LARGE, STYLE_SECONDARY);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);

  AddChildView(label.release());
}

bool PasswordSignInPromoView::Accept() {
  model_->OnSignInToChromeClicked();
  return true;
}

bool PasswordSignInPromoView::Cancel() {
  NOTREACHED();
  return true;
}

int PasswordSignInPromoView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 PasswordSignInPromoView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN);
}
