// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/multiprofiles_intro_dialog.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Default width of the dialog.
const int kDefaultWidth = 448;

}  // namespace

// static
void MultiprofilesIntroDialog::Show(OnAcceptCallback on_accept) {
  MultiprofilesIntroDialog* dialog_view =
      new MultiprofilesIntroDialog(std::move(on_accept));
  dialog_view->InitDialog();
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, Shell::GetRootWindowForNewWindows(), nullptr);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

bool MultiprofilesIntroDialog::Cancel() {
  std::move(on_accept_).Run(false, false);
  return true;
}

bool MultiprofilesIntroDialog::Accept() {
  std::move(on_accept_).Run(true, never_show_again_checkbox_->checked());
  return true;
}

views::View* MultiprofilesIntroDialog::CreateExtraView() {
  return never_show_again_checkbox_;
}

ui::ModalType MultiprofilesIntroDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 MultiprofilesIntroDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_INTRO_HEADLINE);
}

gfx::Size MultiprofilesIntroDialog::CalculatePreferredSize() const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

MultiprofilesIntroDialog::MultiprofilesIntroDialog(OnAcceptCallback on_accept)
    : never_show_again_checkbox_(new views::Checkbox(
          l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_INTRO_NOSHOW_AGAIN))),
      on_accept_(std::move(on_accept)) {
  never_show_again_checkbox_->SetChecked(true);
}

MultiprofilesIntroDialog::~MultiprofilesIntroDialog() = default;

void MultiprofilesIntroDialog::InitDialog() {
  SetBorder(
      views::CreateEmptyBorder(views::LayoutProvider::Get()->GetInsetsMetric(
          views::INSETS_DIALOG_TITLE)));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Explanation string
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_INTRO_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);
}

}  // namespace ash
