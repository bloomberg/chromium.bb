// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_password_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/system/tray/size_range_layout.h"
#include "ash/system/user/button_from_view.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Width/height of the submit button.
constexpr int kSubmitButtonWidthDp = 20;
constexpr int kSubmitButtonHeightDp = 20;

// Width/height of the password inputfield.
constexpr int kPasswordInputWidthDp = 184;
constexpr int kPasswordInputHeightDp = 40;

// Total width of the password view.
constexpr int kPasswordTotalWidthDp = 204;

// Distance between the last password dot and the submit arrow/button.
constexpr int kDistanceBetweenPasswordAndSubmitDp = 0;

const char* kLoginPasswordViewName = "LoginPasswordView";

}  // namespace

LoginPasswordView::TestApi::TestApi(LoginPasswordView* view) : view_(view) {}

LoginPasswordView::TestApi::~TestApi() = default;

views::View* LoginPasswordView::TestApi::textfield() const {
  return view_->textfield_;
}

views::View* LoginPasswordView::TestApi::submit_button() const {
  return view_->submit_button_;
}

LoginPasswordView::LoginPasswordView(const OnPasswordSubmit& on_submit)
    : on_submit_(on_submit) {
  DCHECK(on_submit_);
  auto* root_layout = new views::BoxLayout(views::BoxLayout::kVertical);
  root_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(root_layout);

  auto* row = new views::View();
  AddChildView(row);
  auto* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(),
                           kDistanceBetweenPasswordAndSubmitDp);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  row->SetLayoutManager(layout);

  // Password textfield. We control the textfield size by sizing the parent
  // view, as the textfield will expand to fill it.
  auto* textfield_sizer = new views::View();
  textfield_sizer->SetLayoutManager(new SizeRangeLayout(
      gfx::Size(kPasswordInputWidthDp, kPasswordInputHeightDp)));
  textfield_ = new views::Textfield();
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  // TODO(jdufault): Real placeholder text.
  textfield_->set_placeholder_text(base::ASCIIToUTF16("Password (FIXME)"));
  textfield_->SetBorder(nullptr);

  textfield_sizer->AddChildView(textfield_);
  row->AddChildView(textfield_sizer);

  // Submit button.
  auto* submit_icon = new views::ImageView();
  submit_icon->SetPreferredSize(
      gfx::Size(kSubmitButtonWidthDp, kSubmitButtonHeightDp));
  // TODO(jdufault): Use the real icon.
  submit_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuArrowRightIcon, SK_ColorBLUE));
  submit_button_ = new ash::tray::ButtonFromView(
      submit_icon, this, TrayPopupInkDropStyle::HOST_CENTERED);
  row->AddChildView(submit_button_);

  // Separator on bottom.
  AddChildView(new views::Separator());

  // Make sure the textfield always starts with focus.
  textfield_->RequestFocus();
}

LoginPasswordView::~LoginPasswordView() = default;

void LoginPasswordView::AppendNumber(int value) {
  textfield_->SetText(textfield_->text() + base::IntToString16(value));
}

void LoginPasswordView::Backspace() {
  // Instead of just adjusting textfield_ text directly, fire a backspace key
  // event as this handles the various edge cases (ie, selected text).

  // views::Textfield::OnKeyPressed is private, so we call it via views::View.
  auto* view = static_cast<views::View*>(textfield_);
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
  view->OnKeyPressed(ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_BACK,
                                  ui::DomCode::BACKSPACE, ui::EF_NONE));
}

void LoginPasswordView::Submit() {}

const char* LoginPasswordView::GetClassName() const {
  return kLoginPasswordViewName;
}

gfx::Size LoginPasswordView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();
  size.set_width(kPasswordTotalWidthDp);
  return size;
}

bool LoginPasswordView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    SubmitPassword();
    return true;
  }

  return false;
}

void LoginPasswordView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK_EQ(sender, submit_button_);
  SubmitPassword();
}

void LoginPasswordView::SubmitPassword() {
  on_submit_.Run(textfield_->text());
  textfield_->SetText(base::string16());
}

}  // namespace ash