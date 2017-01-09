// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/multiprofiles_session_aborted_dialog.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

namespace {

// Default width/height of the dialog.
const int kDefaultWidth = 600;
const int kDefaultHeight = 250;

const int kPaddingToMessage = 20;
const int kInset = 40;
const int kTopInset = 10;

////////////////////////////////////////////////////////////////////////////////
// Dialog for an aborted multi-profile session due to a user policy change .
class MultiprofilesSessionAbortedView : public views::DialogDelegateView {
 public:
  MultiprofilesSessionAbortedView();
  ~MultiprofilesSessionAbortedView() override;

  static void ShowDialog(const std::string& user_email);

  // views::DialogDelegate overrides.
  bool Accept() override;
  int GetDialogButtons() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;

 private:
  void InitDialog(const std::string& user_email);

  DISALLOW_COPY_AND_ASSIGN(MultiprofilesSessionAbortedView);
};

////////////////////////////////////////////////////////////////////////////////
// MultiprofilesSessionAbortedView implementation.

MultiprofilesSessionAbortedView::MultiprofilesSessionAbortedView() {
}

MultiprofilesSessionAbortedView::~MultiprofilesSessionAbortedView() {
}

// static
void MultiprofilesSessionAbortedView::ShowDialog(
    const std::string& user_email) {
  MultiprofilesSessionAbortedView* dialog_view =
      new MultiprofilesSessionAbortedView();
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, ash::Shell::GetTargetRootWindow(), NULL);
  dialog_view->InitDialog(user_email);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();

  // Since this is the last thing the user ever sees, we also hide all system
  // tray's from the screen.
  std::vector<ash::RootWindowController*> controllers =
      ash::Shell::GetAllRootWindowControllers();
  for (ash::RootWindowController* controller : controllers) {
    controller->wm_shelf()->SetAutoHideBehavior(
        ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  }
}

bool MultiprofilesSessionAbortedView::Accept() {
  chrome::AttemptUserExit();
  return true;
}

int MultiprofilesSessionAbortedView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 MultiprofilesSessionAbortedView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16(
      IDS_MULTIPROFILES_SESSION_ABORT_BUTTON_LABEL);
}

ui::ModalType MultiprofilesSessionAbortedView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

gfx::Size MultiprofilesSessionAbortedView::GetPreferredSize() const {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

void MultiprofilesSessionAbortedView::InitDialog(
    const std::string& user_email) {
  const gfx::Insets kDialogInsets(kTopInset, kInset, kInset, kInset);

  // Create the views and layout manager and set them up.
  views::GridLayout* grid_layout = views::GridLayout::CreatePanel(this);
  grid_layout->SetInsets(kDialogInsets);

  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  views::Label* title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_MULTIPROFILES_SESSION_ABORT_HEADLINE));
  title_label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumBoldFont));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(title_label_);
  grid_layout->AddPaddingRow(0, kPaddingToMessage);

  // Explanation string.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_MULTIPROFILES_SESSION_ABORT_MESSAGE,
                                 base::ASCIIToUTF16(user_email)));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(label);

  SetLayoutManager(grid_layout);
  Layout();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

void ShowMultiprofilesSessionAbortedDialog(const std::string& user_email) {
  MultiprofilesSessionAbortedView::ShowDialog(user_email);
}

}  // namespace chromeos
