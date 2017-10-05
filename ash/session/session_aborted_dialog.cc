// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_aborted_dialog.h"

#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Default width/height of the dialog.
// TODO(crbug.com/766395): use layout provider for this dialog.
const int kDefaultWidth = 600;
const int kDefaultHeight = 250;

const int kPaddingToMessage = 20;

}  // namespace

// static
void SessionAbortedDialog::Show(const std::string& user_email) {
  SessionAbortedDialog* dialog_view = new SessionAbortedDialog();
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, Shell::GetRootWindowForNewWindows(), nullptr);
  dialog_view->InitDialog(user_email);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();

  // Since this is the last thing the user ever sees, we also hide all system
  // trays from the screen.
  std::vector<RootWindowController*> controllers =
      Shell::GetAllRootWindowControllers();
  for (RootWindowController* controller : controllers) {
    controller->shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  }
}

bool SessionAbortedDialog::Accept() {
  Shell::Get()->session_controller()->RequestSignOut();
  return true;
}

int SessionAbortedDialog::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 SessionAbortedDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_MULTIPROFILES_SESSION_ABORT_BUTTON_LABEL);
}

ui::ModalType SessionAbortedDialog::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

gfx::Size SessionAbortedDialog::CalculatePreferredSize() const {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

SessionAbortedDialog::SessionAbortedDialog() = default;
SessionAbortedDialog::~SessionAbortedDialog() = default;

void SessionAbortedDialog::InitDialog(const std::string& user_email) {
  constexpr int kTopInset = 10;
  constexpr int kOtherInset = 40;
  // Create the views and layout manager and set them up.
  views::GridLayout* grid_layout = views::GridLayout::CreateAndInstall(this);
  SetBorder(views::CreateEmptyBorder(kTopInset, kOtherInset, kOtherInset,
                                     kOtherInset));

  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  views::Label* title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_MULTIPROFILES_SESSION_ABORT_HEADLINE));
  title_label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumBoldFont));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(title_label_);
  grid_layout->AddPaddingRow(0, kPaddingToMessage);

  // Explanation string.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_ASH_MULTIPROFILES_SESSION_ABORT_MESSAGE,
                                 base::ASCIIToUTF16(user_email)));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(label);

  Layout();
}

}  // namespace ash
