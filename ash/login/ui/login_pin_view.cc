// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_pin_view.h"

#include <memory>

#include "ash/login/ui/login_button.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/size.h"
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

// Color of the ink drop highlight.
constexpr SkColor kInkDropHighlightColor =
    SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF);

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
constexpr int kInitialBackspaceDelayMs = 500;
// After the first auto-submit, how long until the next backspace event fires?
constexpr int kRepeatingBackspaceDelayMs = 150;

// Size of the md-ripple when a PIN button is tapped.
constexpr int kRippleSizeDp = 54;

// Button sizes. Button height varies per keyboard style, while button width is
// the same for both styles.
constexpr int kAlphanumericButtonHeightDp = 78;
constexpr int kNumericButtonHeightDp = 70;
constexpr int kButtonWidthDp = 78;

base::string16 GetButtonLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{base::size(kPinLabels)});
  return base::ASCIIToUTF16(std::to_string(value));
}

base::string16 GetButtonSubLabelForNumber(int value) {
  DCHECK(value >= 0 && value < int{base::size(kPinLabels)});
  return base::ASCIIToUTF16(kPinLabels[value]);
}

// Returns the view id for the given pin number.
int GetViewIdForPinNumber(int number) {
  // 0 is a valid pin number but it is also the default view id. Shift all ids
  // over so 0 can be found.
  return number + 1;
}

// A base class for pin button in the pin keyboard.
class BasePinButton : public views::InkDropHostView {
 public:
  explicit BasePinButton(const gfx::Size& size,
                         const base::string16& accessible_name,
                         const base::RepeatingClosure& on_press)
      : on_press_(on_press), accessible_name_(accessible_name) {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetPreferredSize(size);

    auto layout =
        std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
    layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    SetLayoutManager(std::move(layout));

    // Layer rendering is needed for animation. Enable it here for
    // focus painter to paint.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

    focus_painter_ = views::Painter::CreateSolidFocusPainter(
        kFocusBorderColor, kFocusBorderThickness, gfx::InsetsF());
  }

  ~BasePinButton() override = default;

  // views::InkDropHostView:
  void OnPaint(gfx::Canvas* canvas) override {
    InkDropHostView::OnPaint(canvas);
    views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
  }
  void OnFocus() override {
    InkDropHostView::OnFocus();
    SchedulePaint();
  }
  void OnBlur() override {
    InkDropHostView::OnBlur();
    SchedulePaint();
  }
  void OnEvent(ui::Event* event) override {
    bool is_key_press = event->type() == ui::ET_KEY_PRESSED &&
                        (event->AsKeyEvent()->code() == ui::DomCode::ENTER ||
                         event->AsKeyEvent()->code() == ui::DomCode::SPACE);
    bool is_mouse_press = event->type() == ui::ET_MOUSE_PRESSED;
    bool is_gesture_tap = event->type() == ui::ET_GESTURE_TAP_DOWN;

    if (is_key_press || is_mouse_press || is_gesture_tap) {
      DispatchPress(event);
      return;
    }

    views::InkDropHostView::OnEvent(event);
  }
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    node_data->SetName(accessible_name_);
    node_data->role = ax::mojom::Role::kButton;
  }

  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    std::unique_ptr<views::InkDropImpl> ink_drop =
        CreateDefaultFloodFillInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    return std::move(ink_drop);
  }
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override {
    return std::make_unique<views::CircleInkDropMask>(
        size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
  }
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override {
    gfx::Point center = GetLocalBounds().CenterPoint();
    const int radius = GetInkDropRadius();
    gfx::Rect bounds(center.x() - radius, center.y() - radius, radius * 2,
                     radius * 2);

    return std::make_unique<views::FloodFillInkDropRipple>(
        size(), GetLocalBounds().InsetsFrom(bounds),
        GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
        1.f /*visible_opacity*/);
  }
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    return std::make_unique<views::InkDropHighlight>(
        gfx::PointF(GetLocalBounds().CenterPoint()),
        std::make_unique<views::CircleLayerDelegate>(kInkDropHighlightColor,
                                                     GetInkDropRadius()));
  }
  SkColor GetInkDropBaseColor() const override {
    return SkColorSetA(SK_ColorWHITE, 0x0F);
  }

  int GetInkDropRadius() const { return kRippleSizeDp / 2; }

 protected:
  // Called when the button has been pressed.
  virtual void DispatchPress(ui::Event* event) {
    if (on_press_)
      on_press_.Run();
    if (event)
      event->SetHandled();

    AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                   ui::LocatedEvent::FromIfValid(event));
    SchedulePaint();
  }

  // Handler for press events. May be null.
  base::RepeatingClosure on_press_;

 private:
  const base::string16 accessible_name_;
  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(BasePinButton);
};

// A PIN button that displays a digit number and corresponding letter mapping.
class DigitPinButton : public BasePinButton {
 public:
  DigitPinButton(int value,
                 bool show_sub_label,
                 const gfx::Size& size,
                 const LoginPinView::OnPinKey& on_key)
      : BasePinButton(size,
                      GetButtonLabelForNumber(value),
                      base::BindRepeating(on_key, value)) {
    set_id(GetViewIdForPinNumber(value));
    const gfx::FontList& base_font_list = views::Label::GetDefaultFontList();
    views::Label* label = new views::Label(GetButtonLabelForNumber(value),
                                           views::style::CONTEXT_BUTTON,
                                           views::style::STYLE_PRIMARY);
    label->SetEnabledColor(login_constants::kButtonEnabledColor);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetSubpixelRenderingEnabled(false);
    label->SetFontList(base_font_list.Derive(8, gfx::Font::FontStyle::NORMAL,
                                             gfx::Font::Weight::LIGHT));
    AddChildView(label);

    if (show_sub_label) {
      views::Label* sub_label = new views::Label(
          GetButtonSubLabelForNumber(value), views::style::CONTEXT_BUTTON,
          views::style::STYLE_PRIMARY);
      sub_label->SetEnabledColor(
          SkColorSetA(login_constants::kButtonEnabledColor,
                      login_constants::kButtonDisabledAlpha));
      sub_label->SetAutoColorReadabilityEnabled(false);
      sub_label->SetSubpixelRenderingEnabled(false);
      sub_label->SetFontList(base_font_list.Derive(
          -3, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::NORMAL));
      AddChildView(sub_label);
    }
  }

  ~DigitPinButton() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DigitPinButton);
};

}  // namespace

// A PIN button that displays backspace icon.
class LoginPinView::BackspacePinButton : public BasePinButton {
 public:
  BackspacePinButton(const gfx::Size& size,
                     const base::RepeatingClosure& on_press)
      : BasePinButton(size,
                      l10n_util::GetStringUTF16(
                          IDS_ASH_PIN_KEYBOARD_DELETE_ACCESSIBLE_NAME),
                      on_press),
        delay_timer_(std::make_unique<base::OneShotTimer>()),
        repeat_timer_(std::make_unique<base::RepeatingTimer>()) {
    image_ = new views::ImageView();
    image_->SetImage(gfx::CreateVectorIcon(
        kLockScreenBackspaceIcon, login_constants::kButtonEnabledColor));
    AddChildView(image_);
    SetEnabled(false);
  }

  ~BackspacePinButton() override = default;

  void SetTimersForTesting(std::unique_ptr<base::OneShotTimer> delay_timer,
                           std::unique_ptr<base::RepeatingTimer> repeat_timer) {
    delay_timer_ = std::move(delay_timer);
    repeat_timer_ = std::move(repeat_timer);
  }

  // BasePinButton:
  void OnEnabledChanged() override {
    SkColor color = login_constants::kButtonEnabledColor;
    if (!enabled()) {
      color = SkColorSetA(color, login_constants::kButtonDisabledAlpha);
      CancelRepeat();
    }

    image_->SetImage(gfx::CreateVectorIcon(kLockScreenBackspaceIcon, color));
  }
  void OnEvent(ui::Event* event) override {
    BasePinButton::OnEvent(event);
    if (event->handled())
      return;
    // If this is a button release style event cancel any repeat.
    if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
        event->type() == ui::ET_GESTURE_END ||
        event->type() == ui::ET_MOUSE_RELEASED) {
      CancelRepeat();
    }
  }
  void DispatchPress(ui::Event* event) override {
    // Key events have their own repeat logic that is managed by the system.
    if (event && event->IsKeyEvent()) {
      BasePinButton::DispatchPress(event);
      return;
    }

    // If this is the first time the button has been pressed, do not fire a
    // submit even immediately. Instead, trigger the delay timer. The
    // cancellation logic handles the edge case of a button just being tapped.
    if (!is_held_) {
      is_held_ = true;
      DCHECK(!delay_timer_->IsRunning());
      DCHECK(!repeat_timer_->IsRunning());
      delay_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kInitialBackspaceDelayMs),
          base::BindRepeating(&BackspacePinButton::DispatchPress,
                              base::Unretained(this), nullptr));

      if (event)
        event->SetHandled();

      AnimateInkDrop(views::InkDropState::ACTIVATED,
                     ui::LocatedEvent::FromIfValid(event));
      SchedulePaint();

      return;
    }

    // If here, then this function was fired by the delay_timer_. We need to
    // make sure the repeat_timer_ is running so the function will fire again.
    if (!repeat_timer_->IsRunning()) {
      repeat_timer_->Start(
          FROM_HERE,
          base::TimeDelta::FromMilliseconds(kRepeatingBackspaceDelayMs),
          base::BindRepeating(&BackspacePinButton::DispatchPress,
                              base::Unretained(this), nullptr));
    }

    // Run handler.
    if (on_press_)
      on_press_.Run();
  }

 private:
  // Cancels a long-press. If the press event has not been triggered yet this
  // will trigger it.
  void CancelRepeat() {
    if (!is_held_)
      return;

    bool did_submit = !delay_timer_->IsRunning();
    delay_timer_->Stop();
    repeat_timer_->Stop();
    is_held_ = false;

    if (!did_submit && on_press_)
      on_press_.Run();

    AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
    SchedulePaint();
  }

  bool is_held_ = false;
  std::unique_ptr<base::OneShotTimer> delay_timer_;
  std::unique_ptr<base::RepeatingTimer> repeat_timer_;

  views::ImageView* image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BackspacePinButton);
};

// static
gfx::Size LoginPinView::TestApi::GetButtonSize(Style style) {
  return gfx::Size(kButtonWidthDp, style == Style::kNumeric
                                       ? kNumericButtonHeightDp
                                       : kAlphanumericButtonHeightDp);
}

LoginPinView::TestApi::TestApi(LoginPinView* view) : view_(view) {}

LoginPinView::TestApi::~TestApi() = default;

views::View* LoginPinView::TestApi::GetButton(int number) const {
  return view_->GetViewByID(GetViewIdForPinNumber(number));
}

views::View* LoginPinView::TestApi::GetBackspaceButton() const {
  return view_->backspace_;
}

void LoginPinView::TestApi::SetBackspaceTimers(
    std::unique_ptr<base::OneShotTimer> delay_timer,
    std::unique_ptr<base::RepeatingTimer> repeat_timer) {
  view_->backspace_->SetTimersForTesting(std::move(delay_timer),
                                         std::move(repeat_timer));
}

LoginPinView::LoginPinView(Style keyboard_style,
                           const OnPinKey& on_key,
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
    row->SetLayoutManager(
        std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal));
    AddChildView(row);
    return row;
  };

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  bool show_letters = keyboard_style == Style::kAlphanumeric;
  const gfx::Size button_size =
      gfx::Size(kButtonWidthDp, show_letters ? kAlphanumericButtonHeightDp
                                             : kNumericButtonHeightDp);

  auto add_digit_button = [&](View* row, int value) {
    row->AddChildView(
        new DigitPinButton(value, show_letters, button_size, on_key_));
  };

  // 1-2-3
  auto* row = build_and_add_row();
  add_digit_button(row, 1);
  add_digit_button(row, 2);
  add_digit_button(row, 3);

  // 4-5-6
  row = build_and_add_row();
  add_digit_button(row, 4);
  add_digit_button(row, 5);
  add_digit_button(row, 6);

  // 7-8-9
  row = build_and_add_row();
  add_digit_button(row, 7);
  add_digit_button(row, 8);
  add_digit_button(row, 9);

  // 0-backspace
  row = build_and_add_row();
  auto* spacer = new NonAccessibleView();
  spacer->SetPreferredSize(button_size);
  row->AddChildView(spacer);
  add_digit_button(row, 0);
  backspace_ = new BackspacePinButton(button_size, on_backspace_);
  row->AddChildView(backspace_);
}

LoginPinView::~LoginPinView() = default;

void LoginPinView::OnPasswordTextChanged(bool is_empty) {
  // Disabling the backspace button will make it lose focus. The previous
  // focusable view is a button in PIN keyboard, which is slightly more expected
  // than the user menu.
  if (is_empty && backspace_->HasFocus())
    backspace_->GetPreviousFocusableView()->RequestFocus();
  backspace_->SetEnabled(!is_empty);
}

}  // namespace ash
