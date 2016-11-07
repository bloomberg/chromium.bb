// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/multiprofiles_intro_dialog.h"

#include "ash/shell.h"
#include "base/macros.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace chromeos {

namespace {

// Default width of the dialog.
const int kDefaultWidth = 448;

////////////////////////////////////////////////////////////////////////////////
// Dialog for multi-profiles introduction.
class MultiprofilesIntroView : public views::DialogDelegateView {
 public:
  explicit MultiprofilesIntroView(const base::Callback<void(bool)> on_accept);
  ~MultiprofilesIntroView() override;

  static void ShowDialog(const base::Callback<void(bool)> on_accept);

  // views::DialogDelegate overrides.
  bool Accept() override;
  View* CreateExtraView() override;

  // views::WidgetDelegate overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;

 private:
  void InitDialog();

  views::Checkbox* no_show_checkbox_;
  const base::Callback<void(bool)> on_accept_;

  DISALLOW_COPY_AND_ASSIGN(MultiprofilesIntroView);
};

////////////////////////////////////////////////////////////////////////////////
// MultiprofilesIntroDialog implementation.

MultiprofilesIntroView::MultiprofilesIntroView(
    const base::Callback<void(bool)> on_accept)
    : on_accept_(on_accept) {
}

MultiprofilesIntroView::~MultiprofilesIntroView() {
}

// static
void MultiprofilesIntroView::ShowDialog(
    const base::Callback<void(bool)> on_accept) {
  MultiprofilesIntroView* dialog_view =
      new MultiprofilesIntroView(on_accept);
  dialog_view->InitDialog();
  views::DialogDelegate::CreateDialogWidget(
      dialog_view, ash::Shell::GetTargetRootWindow(), NULL);
  views::Widget* widget = dialog_view->GetWidget();
  DCHECK(widget);
  widget->Show();
}

bool MultiprofilesIntroView::Accept() {
  on_accept_.Run(no_show_checkbox_->checked());
  return true;
}

views::View* MultiprofilesIntroView::CreateExtraView() {
  no_show_checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_VISIT_DESKTOP_WARNING_SHOW_DISMISS));
  no_show_checkbox_->SetChecked(true);
  return no_show_checkbox_;
}

ui::ModalType MultiprofilesIntroView::GetModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 MultiprofilesIntroView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_MULTIPROFILES_INTRO_HEADLINE);
}

gfx::Size MultiprofilesIntroView::GetPreferredSize() const {
  return gfx::Size(
      kDefaultWidth,
      GetLayoutManager()->GetPreferredHeightForWidth(this, kDefaultWidth));
}

void MultiprofilesIntroView::InitDialog() {
  SetBorder(views::CreateEmptyBorder(views::kPanelVertMargin,
                                     views::kButtonHEdgeMarginNew, 0,
                                     views::kButtonHEdgeMarginNew));
  SetLayoutManager(new views::FillLayout());

  // Explanation string
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_MULTIPROFILES_INTRO_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  AddChildView(label);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Factory function.

void ShowMultiprofilesIntroDialog(const base::Callback<void(bool)> on_accept) {
  MultiprofilesIntroView::ShowDialog(on_accept);
}

}  // namespace chromeos
