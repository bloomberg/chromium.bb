// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/account_creation_view.h"

#include <algorithm>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/keyboard_codes.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/widget/widget.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"
#include "views/window/window_gtk.h"

using views::Background;
using views::Label;
using views::Textfield;
using views::View;
using views::Widget;

namespace {

const int kCornerRadius = 12;
const int kHorizontalPad = 10;
const int kVerticalInset = 50;
const int kHorizontalInset = 40;
const int kTextfieldWidthInChars = 20;
const SkColor kErrorColor = 0xFF8F384F;
const SkColor kBackground = SK_ColorWHITE;
const SkColor kLabelColor = 0xFF808080;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, public:
AccountCreationView::AccountCreationView(chromeos::ScreenObserver* observer)
    : observer_(observer) {
}

AccountCreationView::~AccountCreationView() {
}

void AccountCreationView::Init() {
  // Use rounded rect background.
  views::Painter* painter = new chromeos::RoundedRectPainter(
      0, kBackground,             // no padding
      true, SK_ColorBLACK,        // black shadow
      kCornerRadius,              // corner radius
      kBackground, kBackground);  // backgound without gradient
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  InitControls();
  InitLayout();
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::View overrides:
gfx::Size AccountCreationView::GetPreferredSize() {
  return gfx::Size(width(), height());
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::WindowDelegate overrides:
views::View* AccountCreationView::GetContentsView() {
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::ButtonListener overrides:
void AccountCreationView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  // TODO(avayvod): Back button.
  CreateAccount();
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, views::Textfield::Controller overrides:
bool AccountCreationView::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    CreateAccount();
    // Return true so that processing ends
    return true;
  }
  // Return false so that processing does not end
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// AccountCreationView, private:
void AccountCreationView::CreateAccount() {
  // TODO(avayvod): Implement account creation.
}

void AccountCreationView::InitLabel(const gfx::Font& label_font,
                                    const std::wstring& label_text,
                                    views::Label** label) {
  scoped_ptr<views::Label> new_label(new views::Label());
  new_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  new_label->SetFont(label_font);
  new_label->SetText(label_text);
  new_label->SetColor(kLabelColor);
  AddChildView(new_label.get());
  *label = new_label.release();
}

void AccountCreationView::InitTextfield(const gfx::Font& field_font,
                                        views::Textfield::StyleFlags style,
                                        views::Textfield** field) {
  scoped_ptr<views::Textfield> new_field(new views::Textfield(style));
  new_field->SetFont(field_font);
  new_field->set_default_width_in_chars(kTextfieldWidthInChars);
  AddChildView(new_field.get());
  new_field->SetController(this);
  *field = new_field.release();
}

void AccountCreationView::InitControls() {
  // Set up fonts.
  gfx::Font title_font =
      gfx::Font::CreateFont(L"Droid Sans", 10).DeriveFont(0, gfx::Font::BOLD);
  gfx::Font label_font = gfx::Font::CreateFont(L"Droid Sans", 8);
  gfx::Font button_font = label_font;
  gfx::Font field_font = label_font;

  InitLabel(title_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_TITLE),
            &title_label_);

  InitLabel(label_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_FIRSTNAME),
            &firstname_label_);
  InitTextfield(field_font, views::Textfield::STYLE_DEFAULT,
                &firstname_field_);
  InitLabel(label_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_LASTNAME),
            &lastname_label_);
  InitTextfield(field_font, views::Textfield::STYLE_DEFAULT, &lastname_field_);
  InitLabel(label_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_EMAIL),
            &username_label_);
  InitTextfield(field_font, views::Textfield::STYLE_DEFAULT, &username_field_);
  InitLabel(label_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_PASSWORD),
            &password_label_);
  InitTextfield(field_font, views::Textfield::STYLE_PASSWORD,
                &password_field_);
  InitLabel(label_font,
            l10n_util::GetString(IDS_ACCOUNT_CREATION_REENTER_PASSWORD),
            &reenter_password_label_);
  InitTextfield(field_font, views::Textfield::STYLE_PASSWORD,
                &reenter_password_field_);

  create_account_button_ = new views::NativeButton(this, std::wstring());
  create_account_button_->set_font(button_font);
  create_account_button_->SetLabel(
      l10n_util::GetString(IDS_ACCOUNT_CREATION_BUTTON));
  AddChildView(create_account_button_);
}

void AccountCreationView::InitLayout() {
  // Create layout for all the views.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // Column for labels.
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::CENTER,
                     0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kHorizontalPad);
  // Column for text fields.
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::CENTER,
                     0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, kHorizontalPad);
  // Column for field status.
  columns->AddColumn(views::GridLayout::LEADING,
                     views::GridLayout::CENTER,
                     1,
                     views::GridLayout::USE_PREF, 0, 0);
  layout->SetInsets(kHorizontalInset,
                    kVerticalInset,
                    kHorizontalInset,
                    kVerticalInset);

  BuildLayout(layout);
}

void AccountCreationView::BuildLayout(views::GridLayout* layout) {
  layout->StartRow(1, 0);
  layout->AddView(firstname_label_);
  layout->AddView(firstname_field_);

  layout->StartRow(1, 0);
  layout->AddView(lastname_label_);
  layout->AddView(lastname_field_);

  layout->StartRow(1, 0);
  layout->AddView(username_label_);
  layout->AddView(username_field_);

  layout->StartRow(1, 0);
  layout->AddView(password_label_);
  layout->AddView(password_field_);

  layout->StartRow(1, 0);
  layout->AddView(reenter_password_label_);
  layout->AddView(reenter_password_field_);

  layout->StartRow(10, 0);
  // Skip columns for label, field and two padding columns in between.
  layout->SkipColumns(4);
  layout->AddView(create_account_button_, 1, 1,
                  views::GridLayout::TRAILING,
                  views::GridLayout::TRAILING);
}

