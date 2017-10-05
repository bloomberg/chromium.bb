// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_view.h"

#include "ash/ash_constants.h"
#include "ash/login/ui/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
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

constexpr const char* kPinLabels[] = {
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

constexpr const char kLoginPinViewClassName[] = "LoginPinView";

// How long does the user have to long-press the backspace button before it
// auto-submits?
const int kInitialBackspaceDelayMs = 500;
// After the first auto-submit, how long until the next backspace event fires?
const int kRepeatingBackspaceDelayMs = 150;

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
class BasePinButton : public views::Button, public views::ButtonListener {
 public:
  explicit BasePinButton(const base::Closure& on_press)
      : views::Button(this), on_press_(on_press) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetPreferredSize(
        gfx::Size(LoginPinView::kButtonSizeDp, LoginPinView::kButtonSizeDp));
    SetFocusPainter(views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF()));
    auto* layout = new views::BoxLayout(views::BoxLayout::kVertical);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(layout);

    SetInkDropMode(InkDropHostView::InkDropMode::ON);
    set_has_ink_drop_action_on_click(true);

    // Layer rendering is needed for animation. Enable it here for
    // focus painter to paint.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
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

 protected:
  base::Closure on_press_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BasePinButton);
};

// A PIN button that displays a digit number and corresponding letter mapping.
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
    label->SetEnabledColor(login_constants::kButtonEnabledColor);
    sub_label->SetEnabledColor(
        SkColorSetA(login_constants::kButtonEnabledColor,
                    login_constants::kButtonDisabledAlpha));
    label->SetAutoColorReadabilityEnabled(false);
    sub_label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    sub_label->SetSubpixelRenderingEnabled(false);
    label->SetFontList(base_font_list.Derive(8, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::LIGHT));
    sub_label->SetFontList(base_font_list.Derive(
        -3, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
    AddChildView(label);
    AddChildView(sub_label);

    SetAccessibleName(GetButtonLabelForNumber(value));
  }

  ~DigitPinButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DigitPinButton);
};

}  // namespace

// static
const int LoginPinView::kButtonSeparatorSizeDp = 30;
// static
const int LoginPinView::kButtonSizeDp = 48;

// A PIN button that displays backspace icon.
class LoginPinView::BackspacePinButton : public BasePinButton {
 public:
  BackspacePinButton(const base::Closure& on_press)
      : BasePinButton(on_press),
        delay_timer_(base::MakeUnique<base::OneShotTimer>()),
        repeat_timer_(base::MakeUnique<base::RepeatingTimer>()) {
    image_ = new views::ImageView();
    AddChildView(image_);

    SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_ASH_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME));
    SetDisabled(true);
  }

  ~BackspacePinButton() override = default;

  void SetTimersForTesting(std::unique_ptr<base::Timer> delay_timer,
                           std::unique_ptr<base::Timer> repeat_timer) {
    delay_timer_ = std::move(delay_timer);
    repeat_timer_ = std::move(repeat_timer);
  }

  void SetDisabled(bool disabled) {
    if (disabled_ == disabled)
      return;
    disabled_ = disabled;
    image_->SetImage(gfx::CreateVectorIcon(
        kLockScreenBackspaceIcon,
        disabled_ ? SkColorSetA(login_constants::kButtonEnabledColor,
                                login_constants::kButtonDisabledAlpha)
                  : login_constants::kButtonEnabledColor));
  }

  // BasePinButton:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    did_autosubmit_ = false;

    if (IsTriggerableEvent(event) && enabled() &&
        HitTestPoint(event.location())) {
      delay_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kInitialBackspaceDelayMs),
          base::Bind(&BackspacePinButton::DispatchRepeatablePressEvent,
                     base::Unretained(this)));
    }

    return BasePinButton::OnMousePressed(event);
  }
  void OnMouseReleased(const ui::MouseEvent& event) override {
    delay_timer_->Stop();
    repeat_timer_->Stop();
    BasePinButton::OnMouseReleased(event);
  }
  void ButtonPressed(Button* sender, const ui::Event& event) override {
    if (did_autosubmit_)
      return;
    BasePinButton::ButtonPressed(sender, event);
  }

 private:
  void DispatchRepeatablePressEvent() {
    // Start repeat timer if this was fired by the initial delay timer.
    if (!repeat_timer_->IsRunning()) {
      repeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kRepeatingBackspaceDelayMs),
          base::Bind(&BackspacePinButton::DispatchRepeatablePressEvent,
                     base::Unretained(this)));
    }

    did_autosubmit_ = true;
    on_press_.Run();
  }

  views::ImageView* image_;
  bool disabled_ = false;
  bool did_autosubmit_ = false;
  std::unique_ptr<base::Timer> delay_timer_;
  std::unique_ptr<base::Timer> repeat_timer_;

  DISALLOW_COPY_AND_ASSIGN(BackspacePinButton);
};

LoginPinView::TestApi::TestApi(LoginPinView* view) : view_(view) {}

LoginPinView::TestApi::~TestApi() = default;

views::View* LoginPinView::TestApi::GetButton(int number) const {
  return view_->GetViewByID(GetViewIdForPinNumber(number));
}

views::View* LoginPinView::TestApi::GetBackspaceButton() const {
  return view_->backspace_;
}

void LoginPinView::TestApi::SetBackspaceTimers(
    std::unique_ptr<base::Timer> delay_timer,
    std::unique_ptr<base::Timer> repeat_timer) {
  view_->backspace_->SetTimersForTesting(std::move(delay_timer),
                                         std::move(repeat_timer));
}

LoginPinView::LoginPinView(const OnPinKey& on_key,
                           const OnPinBackspace& on_backspace)
    : NonAccessibleView(kLoginPinViewClassName),
      on_key_(on_key),
      on_backspace_(on_backspace) {
  DCHECK(on_key_);
  DCHECK(on_backspace_);

  // Layer rendering.
  SetPaintToLayer(ui::LayerType::LAYER_NOT_DRAWN);

  // Builds and returns a new view which contains a row of the PIN keyboard.
  auto build_and_add_row = [this]() {
    auto* row = new NonAccessibleView();
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
  auto* spacer = new NonAccessibleView();
  spacer->SetPreferredSize(gfx::Size(kButtonSizeDp, kButtonSizeDp));
  row->AddChildView(spacer);
  row->AddChildView(new DigitPinButton(0, on_key_));
  backspace_ = new BackspacePinButton(on_backspace_);
  row->AddChildView(backspace_);
}

LoginPinView::~LoginPinView() = default;

void LoginPinView::OnPasswordTextChanged(bool is_empty) {
  backspace_->SetDisabled(is_empty);
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
