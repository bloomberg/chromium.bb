// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_view.h"

#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

const char* kPinLabels[] = {
    "+",      // 0
    " ABC",   // 1
    " DEF",   // 2
    " GHI",   // 3
    " JKL",   // 4
    " MNO",   // 5
    " PQRS",  // 6
    " TUV",   // 7
    " WXYZ",  // 8
    " MNO",   // 9
};

const char* kLoginPinViewClassName = "LoginPinView";

// View ids. Useful for the test api.
const int kBackspaceButtonId = -1;

base::string16 GetButtonLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{arraysize(kPinLabels)});
  return base::ASCIIToUTF16(std::to_string(value) + kPinLabels[value]);
}

// Returns the view id for the given pin number.
int GetViewIdForPinNumber(int number) {
  // 0 is a valid pin number but it is also the default view id. Shift all ids
  // over so 0 can be found.
  return number + 1;
}

// TODO(jdufault): Get pin button visuals to spec.
class PinButton : public views::LabelButton, public views::ButtonListener {
 public:
  PinButton(const base::string16& label, const base::Closure& on_press)
      : views::LabelButton(this, label), on_press_(on_press) {
    SetFocusBehavior(FocusBehavior::ALWAYS);

    SetEnabledTextColors(SK_ColorWHITE);
    SetPreferredSize(
        gfx::Size(LoginPinView::kButtonSizeDp, LoginPinView::kButtonSizeDp));
  }

  PinButton(int value, const LoginPinView::OnPinKey& on_key)
      : PinButton(GetButtonLabelForNumber(value), base::Bind(on_key, value)) {
    set_id(GetViewIdForPinNumber(value));
  }
  ~PinButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK(sender == this);

    if (on_press_)
      on_press_.Run();
  }

 private:
  base::Closure on_press_;

  DISALLOW_COPY_AND_ASSIGN(PinButton);
};

}  // namespace

// static
const int LoginPinView::kButtonSeparatorSizeDp = 30;
// static
const int LoginPinView::kButtonSizeDp = 48;

LoginPinView::TestApi::TestApi(LoginPinView* view) : view_(view) {}

LoginPinView::TestApi::~TestApi() = default;

views::View* LoginPinView::TestApi::GetButton(int number) const {
  return view_->GetViewByID(GetViewIdForPinNumber(number));
}

views::View* LoginPinView::TestApi::GetBackspaceButton() const {
  return view_->GetViewByID(kBackspaceButtonId);
}

LoginPinView::LoginPinView(const OnPinKey& on_key,
                           const OnPinBackspace& on_backspace)
    : on_key_(on_key), on_backspace_(on_backspace) {
  DCHECK(on_key_);
  DCHECK(on_backspace_);

  // Builds and returns a new view which contains a row of the PIN keyboard.
  auto build_and_add_row = [this]() {
    auto* row = new views::View();
    row->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, gfx::Insets(), kButtonSeparatorSizeDp));
    AddChildView(row);
    return row;
  };

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        gfx::Insets(), kButtonSeparatorSizeDp));

  // 1-2-3
  auto* row = build_and_add_row();
  row->AddChildView(new PinButton(1, on_key_));
  row->AddChildView(new PinButton(2, on_key_));
  row->AddChildView(new PinButton(3, on_key_));

  // 4-5-6
  row = build_and_add_row();
  row->AddChildView(new PinButton(4, on_key_));
  row->AddChildView(new PinButton(5, on_key_));
  row->AddChildView(new PinButton(6, on_key_));

  // 7-8-9
  row = build_and_add_row();
  row->AddChildView(new PinButton(7, on_key_));
  row->AddChildView(new PinButton(8, on_key_));
  row->AddChildView(new PinButton(9, on_key_));

  // 0-backspace
  row = build_and_add_row();
  auto* spacer = new views::View();
  spacer->SetPreferredSize(gfx::Size(kButtonSizeDp, kButtonSizeDp));
  row->AddChildView(spacer);
  row->AddChildView(new PinButton(0, on_key_));
  auto* backspace = new PinButton(base::ASCIIToUTF16("x"), on_backspace_);
  backspace->set_id(kBackspaceButtonId);
  row->AddChildView(backspace);
}

LoginPinView::~LoginPinView() = default;

const char* LoginPinView::GetClassName() const {
  return kLoginPinViewClassName;
}

bool LoginPinView::OnKeyPressed(const ui::KeyEvent& event) {
  // TODO: figure out what to do here.
  if (event.key_code() == ui::KeyboardCode::VKEY_RETURN) {
    // TODO: Real pin.
    // on_submit_.Run(base::ASCIIToUTF16("1111"));
    return true;
  }

  return false;
}

}  // namespace ash