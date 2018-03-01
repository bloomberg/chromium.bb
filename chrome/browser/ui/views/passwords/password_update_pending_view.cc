// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_update_pending_view.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/passwords/credentials_selection_view.h"
#include "chrome/browser/ui/views/passwords/password_items_view.h"
#include "chrome/browser/ui/views/passwords/password_pending_view.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

PasswordUpdatePendingView::PasswordUpdatePendingView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    DisplayReason reason)
    : PasswordBubbleViewBase(web_contents, anchor_view, anchor_point, reason),
      selection_view_(nullptr) {
  // Credential row.
  if (model()->ShouldShowMultipleAccountUpdateUI()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    selection_view_ = new CredentialsSelectionView(model());
    AddChildView(selection_view_);
  } else {
    const autofill::PasswordForm& password_form = model()->pending_password();
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>(this));
    PasswordPendingView::BuildCredentialRows(
        layout, CreateUsernameLabel(password_form).release(),
        CreatePasswordLabel(password_form,
                            IDS_PASSWORD_MANAGER_SIGNIN_VIA_FEDERATION, false)
            .release(),
        nullptr, /* password_view_button */
        true /* show_password_label */);
  }
}

PasswordUpdatePendingView::~PasswordUpdatePendingView() = default;

gfx::Size PasswordUpdatePendingView::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    margins().width();
  return gfx::Size(width, GetHeightForWidth(width));
}

base::string16 PasswordUpdatePendingView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(button == ui::DIALOG_BUTTON_OK
                                       ? IDS_PASSWORD_MANAGER_UPDATE_BUTTON
                                       : IDS_PASSWORD_MANAGER_CANCEL_BUTTON);
}

bool PasswordUpdatePendingView::Accept() {
  if (selection_view_) {
    // Multi account case.
    model()->OnUpdateClicked(*selection_view_->GetSelectedCredentials());
  } else {
    model()->OnUpdateClicked(model()->pending_password());
  }
  return true;
}

bool PasswordUpdatePendingView::Cancel() {
  model()->OnNopeUpdateClicked();
  return true;
}

bool PasswordUpdatePendingView::Close() {
  return true;
}

void PasswordUpdatePendingView::AddedToWidget() {
  auto title_view =
      std::make_unique<views::StyledLabel>(base::string16(), this);
  title_view->SetTextContext(views::style::CONTEXT_DIALOG_TITLE);
  UpdateTitleText(title_view.get());
  GetBubbleFrameView()->SetTitleView(std::move(title_view));
}

void PasswordUpdatePendingView::StyledLabelLinkClicked(
    views::StyledLabel* label,
    const gfx::Range& range,
    int event_flags) {
  DCHECK_EQ(model()->title_brand_link_range(), range);
  model()->OnBrandLinkClicked();
}

void PasswordUpdatePendingView::UpdateTitleText(
    views::StyledLabel* title_view) {
  title_view->SetText(GetWindowTitle());
  if (!model()->title_brand_link_range().is_empty()) {
    auto link_style = views::StyledLabel::RangeStyleInfo::CreateForLink();
    link_style.disable_line_wrapping = false;
    title_view->AddStyleRange(model()->title_brand_link_range(), link_style);
  }
}
