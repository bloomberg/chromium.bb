// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_sign_in_promo_view.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"

ManagePasswordSignInPromoView::ManagePasswordSignInPromoView(
    ManagePasswordsBubbleModel* model)
    : model_(model) {
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
}

bool ManagePasswordSignInPromoView::Accept() {
  model_->OnSignInToChromeClicked();
  return true;
}

bool ManagePasswordSignInPromoView::Cancel() {
  model_->OnSkipSignInClicked();
  return true;
}

base::string16 ManagePasswordSignInPromoView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      button == ui::DIALOG_BUTTON_OK
          ? IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN
          : IDS_PASSWORD_MANAGER_SIGNIN_PROMO_NO_THANKS);
}
