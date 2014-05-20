// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/multi_user/multi_user_warning_dialog.h"

#include "ash/shell.h"
#include "grit/generated_resources.h"
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

const int kPaddingToMessage = 30;
const int kPaddingToCheckBox = 50;
const int kInset = 40;
const int kTopInset = 10;

////////////////////////////////////////////////////////////////////////////////
// Dialog for multi-profiles teleport warning.
class TeleportWarningView : public views::DialogDelegateView {
 public:
  TeleportWarningView(base::Callback<void(bool)> on_accept);
  virtual ~TeleportWarningView();

  static void ShowDialog(const base::Callback<void(bool)> on_accept);

  // views::DialogDelegate overrides.
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate overrides.
  virtual ui::ModalType GetModalType() const OVERRIDE;

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  void InitDialog();

  scoped_ptr<views::Checkbox> no_show_checkbox_;
  const base::Callback<void(bool)> on_accept_;

  DISALLOW_COPY_AND_ASSIGN(TeleportWarningView);
};

////////////////////////////////////////////////////////////////////////////////
// TeleportWarningView implementation.

TeleportWarningView::TeleportWarningView(
    const base::Callback<void(bool)> on_accept)
    : on_accept_(on_accept) {
}

TeleportWarningView::~TeleportWarningView() {
}

// static
void TeleportWarningView::ShowDialog(
    const base::Callback<void(bool)> on_accept) {
  TeleportWarningView* dialog_view =
      new TeleportWarningView(on_accept);
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, ash::Shell::GetTargetRootWindow(), NULL);
  dialog_view->InitDialog();
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

bool TeleportWarningView::Accept() {
  on_accept_.Run(no_show_checkbox_->checked());
  return true;
}

ui::ModalType TeleportWarningView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

gfx::Size TeleportWarningView::GetPreferredSize() const {
  return gfx::Size(kDefaultWidth, kDefaultHeight);
}

void TeleportWarningView::InitDialog() {
  const gfx::Insets kDialogInsets(kTopInset, kInset, kInset, kInset);

  // Create the views and layout manager and set them up.
  views::GridLayout* grid_layout = views::GridLayout::CreatePanel(this);
  grid_layout->SetInsets(kDialogInsets);

  views::ColumnSet* column_set = grid_layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  // Title
  views::Label* title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_VISIT_DESKTOP_WARNING_TITLE));
  title_label_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumBoldFont));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(title_label_);
  grid_layout->AddPaddingRow(0, kPaddingToMessage);

  // Explanation string
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_VISIT_DESKTOP_WARNING_MESSAGE));
  label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(label);

  // Next explanation string
  grid_layout->AddPaddingRow(0, kPaddingToMessage);
  views::Label* lower_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_VISIT_DESKTOP_WARNING_EXPLANATION));
  lower_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  lower_label->SetMultiLine(true);
  lower_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  lower_label->SetAllowCharacterBreak(true);
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(lower_label);

  // No-show again checkbox
  grid_layout->AddPaddingRow(0, kPaddingToCheckBox);
  no_show_checkbox_.reset(new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_VISIT_DESKTOP_WARNING_SHOW_DISMISS)));
  no_show_checkbox_->SetChecked(true);
  no_show_checkbox_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));
  grid_layout->StartRow(0, 0);
  grid_layout->AddView(no_show_checkbox_.get());

  SetLayoutManager(grid_layout);
  Layout();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

void ShowMultiprofilesWarningDialog(
   const base::Callback<void(bool)> on_accept) {
  TeleportWarningView::ShowDialog(on_accept);
}

}  // namespace chromeos
