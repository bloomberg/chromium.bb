// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_password_save_confirmation_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"

ManagePasswordSaveConfirmationView::ManagePasswordSaveConfirmationView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  layout->set_minimum_size(
      gfx::Size(ManagePasswordsBubbleView::kDesiredBubbleWidth, 0));

  views::StyledLabel* confirmation =
      new views::StyledLabel(parent_->model()->save_confirmation_text(), this);
  confirmation->SetTextContext(CONTEXT_DEPRECATED_SMALL);
  auto link_style = views::StyledLabel::RangeStyleInfo::CreateForLink();
  link_style.disable_line_wrapping = false;
  confirmation->AddStyleRange(parent_->model()->save_confirmation_link_range(),
                              link_style);

  ManagePasswordsBubbleView::BuildColumnSet(
      layout, ManagePasswordsBubbleView::SINGLE_VIEW_COLUMN_SET);
  layout->StartRow(0, ManagePasswordsBubbleView::SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_OK));

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  layout->AddPaddingRow(0,
                        layout_provider->GetDistanceMetric(
                            views::DISTANCE_DIALOG_CONTENT_MARGIN_BOTTOM_TEXT));
  ManagePasswordsBubbleView::BuildColumnSet(
      layout, ManagePasswordsBubbleView::SINGLE_BUTTON_COLUMN_SET);
  layout->StartRow(0, ManagePasswordsBubbleView::SINGLE_BUTTON_COLUMN_SET);
  layout->AddView(ok_button_);

  parent_->set_initially_focused_view(ok_button_);
}

ManagePasswordSaveConfirmationView::~ManagePasswordSaveConfirmationView() =
    default;

void ManagePasswordSaveConfirmationView::ButtonPressed(views::Button* sender,
                                                       const ui::Event& event) {
  DCHECK_EQ(sender, ok_button_);
  parent_->model()->OnOKClicked();
  parent_->CloseBubble();
}

void ManagePasswordSaveConfirmationView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(range, parent_->model()->save_confirmation_link_range());
  parent_->model()->OnNavigateToPasswordManagerAccountDashboardLinkClicked();
  parent_->CloseBubble();
}
