// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/sync/one_click_signin_dialog.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

// Heading font size correction.
const int kHeadingFontSizeDelta = 1;
const int kMinColumnWidth = 320;

// A dialog explaining to the user more about one click signin, and provides
// a way for the user to continue or backout.
class OneClickSigninDialogView : public views::DialogDelegateView {
 public:
  explicit OneClickSigninDialogView(
      const OneClickAcceptCallback& accept_callback);
  virtual ~OneClickSigninDialogView();

 private:
  // views::DialogDelegateView:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  OneClickAcceptCallback accept_callback_;

  views::Checkbox* checkbox_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninDialogView);
};

OneClickSigninDialogView::OneClickSigninDialogView(
    const OneClickAcceptCallback& accept_callback)
    : accept_callback_(accept_callback),
      checkbox_(NULL) {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,  // resizable
                        views::GridLayout::USE_PREF,
                        0,  // ignored for USE_PREF
                        kMinColumnWidth);

  layout->StartRow(0, kColumnSetId);
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_HEADING));
  label->SetFont(label->font().DeriveFont(kHeadingFontSizeDelta,
                                          gfx::Font::BOLD));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  checkbox_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_CHECKBOX));
  checkbox_->SetChecked(true);
  layout->AddView(checkbox_);
}

OneClickSigninDialogView::~OneClickSigninDialogView() {
}

string16 OneClickSigninDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON : IDS_CANCEL);
}

int OneClickSigninDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_OK;
}

bool OneClickSigninDialogView::Cancel() {
  return true;
}

bool OneClickSigninDialogView::Accept() {
  const bool use_default_settings = checkbox_->checked();
  accept_callback_.Run(use_default_settings);
  return true;
}

ui::ModalType OneClickSigninDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

string16 OneClickSigninDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE);
}

views::View* OneClickSigninDialogView::GetContentsView() {
  return this;
}

}  // namespace

void ShowOneClickSigninDialog(gfx::NativeWindow parent_window,
                              const OneClickAcceptCallback& accept_callback) {
  OneClickSigninDialogView* dialog =
      new OneClickSigninDialogView(accept_callback);

  views::Widget::CreateWindowWithParent(dialog, parent_window)->Show();
}
