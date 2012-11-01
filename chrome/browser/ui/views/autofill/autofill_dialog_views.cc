// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_template.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace autofill {

namespace {

// Returns a label that describes a details section.
views::Label* CreateSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  label->set_border(views::Border::CreateEmptyBorder(4, 0, 0, 0));
  return label;
}

}  // namespace

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(NULL) {
  DCHECK(controller);
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. The ConstrainedWindow
  // will take care of deleting itself after calling DeleteDelegate().
  window_ = new ConstrainedWindowViews(
      controller_->web_contents(), this,
      true, ConstrainedWindowViews::DEFAULT_INSETS);
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed(AutofillDialogController::AUTOFILL_ACTION_ABORT);
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonEnabled() : true;
}

bool AutofillDialogViews::UseChromeStyle() const {
  return true;
}

bool AutofillDialogViews::Cancel() {
  return true;
}

bool AutofillDialogViews::Accept() {
  NOTREACHED();
  return true;
}

void AutofillDialogViews::InitChildViews() {
  contents_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  // A column set for things that span the whole dialog.
  const int single_column_set = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(0, single_column_set);
  views::Label* intro = new views::Label(controller_->IntroText());
  intro->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(intro);

  // A column set for input fields.
  const int info_column_set = 1;
  column_set = layout->AddColumnSet(info_column_set);
  // TODO(estade): pull out these constants, and figure out better values
  // for them.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        180,
                        0);
  column_set->AddPaddingColumn(0, 15);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        300,
                        0);

  // Email section.
  layout->StartRowWithPadding(0, info_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(CreateSectionLabel(controller_->EmailSectionLabel()));
  layout->AddView(CreateEmailSection());

  // Billing section.
  layout->StartRowWithPadding(0, info_column_set,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(CreateSectionLabel(controller_->BillingSectionLabel()));
  layout->AddView(CreateBillingSection());

  // Separator.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(new views::Separator());

  // Wallet checkbox.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(new views::Checkbox(controller_->WalletOptionText()));
}

views::View* AutofillDialogViews::CreateEmailSection() {
  views::Textfield* field = new views::Textfield();
  field->set_placeholder_text(ASCIIToUTF16("placeholder text"));
  return field;
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::CreateBillingSection() {
  views::View* billing = new views::View();
  views::GridLayout* layout = new views::GridLayout(billing);
  billing->SetLayoutManager(layout);

  for (size_t i = 0; i < kBillingInputsSize; ++i) {
    const DetailInput& input = kBillingInputs[i];
    if (!controller_->ShouldShowInput(input))
      continue;

    int column_set_id = input.row_id;
    views::ColumnSet* column_set = layout->GetColumnSet(column_set_id);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(column_set_id);
      if (billing->has_children())
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, column_set_id);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    float expand = input.expand_weight;
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::BASELINE,
                          expand ? expand : 1,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    views::Textfield* field = new views::Textfield();
    field->set_placeholder_text(ASCIIToUTF16(input.placeholder_text));
    layout->AddView(field);
  }

  return billing;
}

}  // namespace autofill
