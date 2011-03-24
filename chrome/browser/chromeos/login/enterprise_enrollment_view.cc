// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enterprise_enrollment_view.h"

#include "chrome/browser/chromeos/login/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

namespace chromeos {

namespace {

// Layout constants.
const int kBorderSize = 10;
const int kMargin = 20;

// Column set identifiers.
enum kLayoutColumnsets {
  SINGLE_COLUMN,
  DIALOG_BUTTONS,
};

}  // namespace

EnterpriseEnrollmentView::EnterpriseEnrollmentView(ScreenObserver* observer) {}

EnterpriseEnrollmentView::~EnterpriseEnrollmentView() {}

void EnterpriseEnrollmentView::Init() {
  // Use rounded rect background.
  views::Painter* painter =
      CreateWizardPainter(&BorderDefinition::kScreenBorder);
  set_background(views::Background::CreateBackgroundPainter(true, painter));

  // Initialize the layout.
  views::GridLayout* layout = CreateLayout();
  SetLayoutManager(layout);

  // Create controls.
  layout->AddPaddingRow(0, kBorderSize + kMargin);

  layout->StartRow(0, SINGLE_COLUMN);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font label_font = rb.GetFont(ResourceBundle::MediumFont);
  title_label_ = new views::Label(std::wstring(), label_font);
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(title_label_);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(1, SINGLE_COLUMN);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, DIALOG_BUTTONS);
  layout->SkipColumns(1);
  cancel_button_ = new login::WideButton(this, std::wstring());
  layout->AddView(cancel_button_);

  layout->AddPaddingRow(0, kBorderSize + kMargin);

  UpdateLocalizedStrings();
}

void EnterpriseEnrollmentView::UpdateLocalizedStrings() {
  title_label_->SetText(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_TITLE)));
  cancel_button_->SetLabel(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_ENTERPRISE_ENROLLMENT_CANCEL)));
}

views::GridLayout* EnterpriseEnrollmentView::CreateLayout() {
  views::GridLayout* layout = new views::GridLayout(this);
  const int kPadding = kBorderSize + kMargin;

  views::ColumnSet* single_column = layout->AddColumnSet(SINGLE_COLUMN);
  single_column->AddPaddingColumn(0, kPadding);
  single_column->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                           1.0, views::GridLayout::USE_PREF, 0, 0);
  single_column->AddPaddingColumn(0, kPadding);

  views::ColumnSet* button_column = layout->AddColumnSet(DIALOG_BUTTONS);
  button_column->AddPaddingColumn(0, kPadding);
  button_column->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                           1.0, views::GridLayout::USE_PREF, 0, 0);
  button_column->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  button_column->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                           views::GridLayout::USE_PREF, 0, 0);
  button_column->AddPaddingColumn(0, kPadding);

  return layout;
}

void EnterpriseEnrollmentView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
}

void EnterpriseEnrollmentView::ButtonPressed(views::Button* sender,
                                             const views::Event& event) {
  if (sender == cancel_button_) {
    if (controller_)
      controller_->CancelEnrollment();
  }
}

}  // namespace chromeos
