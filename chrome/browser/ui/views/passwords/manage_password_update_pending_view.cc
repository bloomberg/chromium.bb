// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_update_pending_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"

ManagePasswordUpdatePendingView::ManagePasswordUpdatePendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent), selection_view_(nullptr) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  layout->set_minimum_size(
      gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth, 0));

  // Credential row.
  if (parent->model()->ShouldShowMultipleAccountUpdateUI()) {
    ManagePasswordsBubbleView::BuildColumnSet(
        layout, ManagePasswordsBubbleView::SINGLE_VIEW_COLUMN_SET);
    layout->StartRow(0, ManagePasswordsBubbleView::SINGLE_VIEW_COLUMN_SET);
    layout->AddView(new CredentialsSelectionView(parent->model()));
  } else {
    const autofill::PasswordForm& password_form =
        parent_->model()->pending_password();
    ManagePasswordsBubbleView::BuildCredentialRows(
        layout, CreateUsernameLabel(password_form).release(),
        CreatePasswordLabel(password_form,
                            IDS_PASSWORD_MANAGER_SIGNIN_VIA_FEDERATION, false)
            .release(),
        nullptr, /* password_view_button */
        true /* show_password_label */);
  }
  layout->AddPaddingRow(
      0, layout_provider->GetDistanceMetric(
             views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_CONTROL));

  // Button row.
  nope_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));

  update_button_ = views::MdTextButton::CreateSecondaryUiBlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UPDATE_BUTTON));
  ManagePasswordsBubbleView::BuildColumnSet(
      layout, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, ManagePasswordsBubbleView::DOUBLE_BUTTON_COLUMN_SET);
  layout->AddView(update_button_);
  layout->AddView(nope_button_);

  parent_->set_initially_focused_view(update_button_);
}

ManagePasswordUpdatePendingView::~ManagePasswordUpdatePendingView() = default;

void ManagePasswordUpdatePendingView::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  DCHECK(sender == update_button_ || sender == nope_button_);
  if (sender == update_button_) {
    if (selection_view_) {
      // Multi account case.
      parent_->model()->OnUpdateClicked(
          *selection_view_->GetSelectedCredentials());
    } else {
      parent_->model()->OnUpdateClicked(parent_->model()->pending_password());
    }
  } else {
    parent_->model()->OnNopeUpdateClicked();
  }
  parent_->CloseBubble();
}

gfx::Size ManagePasswordUpdatePendingView::CalculatePreferredSize() const {
  return gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth,
                   GetLayoutManager()->GetPreferredHeightForWidth(
                       this, ManagePasswordsBubbleView::kDesiredBubbleWidth));
}
