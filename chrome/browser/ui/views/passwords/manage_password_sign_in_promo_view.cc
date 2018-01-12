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
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  layout->set_minimum_size(
      gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth, 0));

  signin_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN));
  no_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_NO_THANKS));

  // Button row.
  ManagePasswordsBubbleView::BuildColumnSet(
      layout, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->AddView(signin_button_);
  layout->AddView(no_button_);

  parent_->set_initially_focused_view(signin_button_);
  base::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
}

void ManagePasswordSignInPromoView::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  if (sender == signin_button_)
    parent_->model()->OnSignInToChromeClicked();
  else if (sender == no_button_)
    parent_->model()->OnSkipSignInClicked();
  else
    NOTREACHED();

  parent_->CloseBubble();
}
