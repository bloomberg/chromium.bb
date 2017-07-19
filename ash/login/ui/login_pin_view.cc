// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_view.h"

#include "ash/ash_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

const char* kPinLabels[] = {
    "+",      // 0
    "",       // 1
    " ABC",   // 2
    " DEF",   // 3
    " GHI",   // 4
    " JKL",   // 5
    " MNO",   // 6
    " PQRS",  // 7
    " TUV",   // 8
    " WXYZ",  // 9
};

const char* kLoginPinViewClassName = "LoginPinView";

// View ids. Useful for the test api.
const int kBackspaceButtonId = -1;

// An alpha value for button's sub label.
// In specs this is listed as 34% = 0x57 / 0xFF.
const SkAlpha kButtonSubLabelAlpha = 0x57;

// Color of the ink drop ripple.
constexpr SkColor kInkDropRippleColor =
    SkColorSetARGBMacro(0xF, 0xFF, 0xFF, 0xFF);

// Color of the ink drop highlight.
constexpr SkColor kInkDropHighlightColor =
    SkColorSetARGBMacro(0x14, 0xFF, 0xFF, 0xFF);

base::string16 GetButtonLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{arraysize(kPinLabels)});
  return base::ASCIIToUTF16(std::to_string(value));
}

base::string16 GetButtonSubLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{arraysize(kPinLabels)});
  return base::ASCIIToUTF16(kPinLabels[value]);
}

// Returns the view id for the given pin number.
int GetViewIdForPinNumber(int number) {
  // 0 is a valid pin number but it is also the default view id. Shift all ids
  // over so 0 can be found.
  return number + 1;
}

// A base class for pin button in the pin keyboard.
class BasePinButton : public views::CustomButton, public views::ButtonListener {
 public:
  explicit BasePinButton(const base::Closure& on_press)
      : views::CustomButton(this), on_press_(on_press) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetPreferredSize(
        gfx::Size(LoginPinView::kButtonSizeDp, LoginPinView::kButtonSizeDp));
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        ash::kFocusBorderColor, ash::kFocusBorderThickness, gfx::InsetsF()));
    auto* layout = new views::BoxLayout(views::BoxLayout::kVertical);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(layout);

    SetInkDropMode(InkDropHostView::InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);
  }

  ~BasePinButton() override = default;

  // views::ButtonListener:
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    DCHECK(sender == this);

    if (on_press_)
      on_press_.Run();
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        base::MakeUnique<views::InkDropImpl>(this, size());
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(false);
    ink_drop->SetAutoHighlightMode(
        views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return base::MakeUnique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(),
        LoginPinView::kButtonSizeDp / 2);
  }

  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    gfx::Rect bounds(center.x() - LoginPinView::kButtonSizeDp / 2,
                     center.y() - LoginPinView::kButtonSizeDp / 2,
                     LoginPinView::kButtonSizeDp, LoginPinView::kButtonSizeDp);

    return base::MakeUnique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor,
        1.f /*visible_opacity*/);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return base::MakeUnique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        base::MakeUnique<views::CircleLayerDelegate>(
            kInkDropHighlightColor, LoginPinView::kButtonSizeDp / 2));
  }

 private:
  base::Closure on_press_;

  DISALLOW_COPY_AND_ASSIGN(BasePinButton);
};

// A pin button that displays a digit number and corresponding letter mapping.
class DigitPinButton : public BasePinButton {
 public:
  DigitPinButton(int value, const LoginPinView::OnPinKey& on_key)
      : BasePinButton(base::Bind(on_key, value)) {
    set_id(GetViewIdForPinNumber(value));
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    views::Label* label = new views::Label(GetButtonLabelForNumber(value),
                                           views::style::CONTEXT_BUTTON,
                                           views::style::STYLE_PRIMARY);
    views::Label* sub_label = new views::Label(
        GetButtonSubLabelForNumber(value), views::style::CONTEXT_BUTTON,
        views::style::STYLE_PRIMARY);
    label->SetEnabledColor(SK_ColorWHITE);
    sub_label->SetEnabledColor(
        SkColorSetA(SK_ColorWHITE, kButtonSubLabelAlpha));
    label->SetAutoColorReadabilityEnabled(false);
    sub_label->SetAutoColorReadabilityEnabled(false);
    label->SetFontList(base_font_list.Derive(8, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::LIGHT));
    sub_label->SetFontList(base_font_list.Derive(
        -3, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    AddChildView(label);
    AddChildView(sub_label);
  }

  ~DigitPinButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DigitPinButton);
};

// A pin button that displays backspace icon.
class BackspacePinButton : public BasePinButton {
 public:
  BackspacePinButton(const base::Closure& on_press) : BasePinButton(on_press) {
    views::ImageView* image = new views::ImageView();
    // TODO: Change icon color when enabled/disabled.
    image->SetImage(
        gfx::CreateVectorIcon(kLockScreenBackspaceIcon, SK_ColorWHITE));
    AddChildView(image);
  }

  ~BackspacePinButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackspacePinButton);
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
  row->AddChildView(new DigitPinButton(1, on_key_));
  row->AddChildView(new DigitPinButton(2, on_key_));
  row->AddChildView(new DigitPinButton(3, on_key_));

  // 4-5-6
  row = build_and_add_row();
  row->AddChildView(new DigitPinButton(4, on_key_));
  row->AddChildView(new DigitPinButton(5, on_key_));
  row->AddChildView(new DigitPinButton(6, on_key_));

  // 7-8-9
  row = build_and_add_row();
  row->AddChildView(new DigitPinButton(7, on_key_));
  row->AddChildView(new DigitPinButton(8, on_key_));
  row->AddChildView(new DigitPinButton(9, on_key_));

  // 0-backspace
  row = build_and_add_row();
  auto* spacer = new views::View();
  spacer->SetPreferredSize(gfx::Size(kButtonSizeDp, kButtonSizeDp));
  row->AddChildView(spacer);
  row->AddChildView(new DigitPinButton(0, on_key_));
  auto* backspace = new BackspacePinButton(on_backspace_);
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
