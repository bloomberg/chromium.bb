// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"

#include <cmath>
#include <map>

#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_size_button.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/strings/grit/ui_strings.h"  // Accessibility names
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

// Duration of the animation of the position of |minimize_button_|.
const int kPositionAnimationDurationMs = 500;

// Duration of the animation of the alpha of |size_button_|.
const int kAlphaAnimationDurationMs = 250;

// Delay during |tablet_mode_animation_| hide to wait before beginning to
// animate the position of |minimize_button_|.
const int kHidePositionDelayMs = 100;

// Duration of |tablet_mode_animation_| hiding.
// Hiding size button 250
// |------------------------|
// Delay 100      Slide minimize button 500
// |---------|-------------------------------------------------|
const int kHideAnimationDurationMs =
    kHidePositionDelayMs + kPositionAnimationDurationMs;

// Delay during |tablet_mode_animation_| show to wait before beginning to
// animate the alpha of |size_button_|.
const int kShowAnimationAlphaDelayMs = 100;

// Duration of |tablet_mode_animation_| showing.
// Slide minimize button 500
// |-------------------------------------------------|
// Delay 100   Show size button 250
// |---------|-----------------------|
const int kShowAnimationDurationMs = kPositionAnimationDurationMs;

// Value of |tablet_mode_animation_| showing to begin animating alpha of
// |size_button_|.
float SizeButtonShowStartValue() {
  return static_cast<float>(kShowAnimationAlphaDelayMs) /
         kShowAnimationDurationMs;
}

// Amount of |tablet_mode_animation_| showing to animate the alpha of
// |size_button_|.
float SizeButtonShowDuration() {
  return static_cast<float>(kAlphaAnimationDurationMs) /
         kShowAnimationDurationMs;
}

// Amount of |tablet_mode_animation_| hiding to animate the alpha of
// |size_button_|.
float SizeButtonHideDuration() {
  return static_cast<float>(kAlphaAnimationDurationMs) /
         kHideAnimationDurationMs;
}

// Value of |tablet_mode_animation_| hiding to begin animating the position of
// |minimize_button_|.
float HidePositionStartValue() {
  return 1.0f -
         static_cast<float>(kHidePositionDelayMs) / kHideAnimationDurationMs;
}

// Converts |point| from |src| to |dst| and hittests against |dst|.
bool ConvertPointToViewAndHitTest(const views::View* src,
                                  const views::View* dst,
                                  const gfx::Point& point) {
  gfx::Point converted(point);
  views::View::ConvertPointToTarget(src, dst, &converted);
  return dst->HitTestPoint(converted);
}

// Bounds animation values to the range 0.0 - 1.0. Allows for mapping of offset
// animations to the expected range so that gfx::Tween::CalculateValue() can be
// used.
double CapAnimationValue(double value) {
  return std::min(1.0, std::max(0.0, value));
}

}  // namespace

// static
const char FrameCaptionButtonContainerView::kViewClassName[] =
    "FrameCaptionButtonContainerView";

FrameCaptionButtonContainerView::FrameCaptionButtonContainerView(
    views::Widget* frame)
    : frame_(frame),
      minimize_button_(NULL),
      size_button_(NULL),
      close_button_(NULL) {
  auto layout =
      std::make_unique<views::BoxLayout>(views::BoxLayout::kHorizontal);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout.release());
  bool size_button_visibility = ShouldSizeButtonBeVisible();
  tablet_mode_animation_.reset(new gfx::SlideAnimation(this));
  tablet_mode_animation_->SetTweenType(gfx::Tween::LINEAR);

  // Ensure animation tracks visibility of size button.
  if (size_button_visibility)
    tablet_mode_animation_->Reset(1.0f);

  // Insert the buttons left to right.
  minimize_button_ = new FrameCaptionButton(this, CAPTION_BUTTON_ICON_MINIMIZE);
  minimize_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
  minimize_button_->SetVisible(frame_->widget_delegate()->CanMinimize());
  AddChildView(minimize_button_);

  size_button_ = new FrameSizeButton(this, frame, this);
  size_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
  size_button_->SetVisible(size_button_visibility);
  AddChildView(size_button_);

  close_button_ = new FrameCaptionButton(this, CAPTION_BUTTON_ICON_CLOSE);
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  AddChildView(close_button_);
}

FrameCaptionButtonContainerView::~FrameCaptionButtonContainerView() = default;

void FrameCaptionButtonContainerView::TestApi::EndAnimations() {
  container_view_->tablet_mode_animation_->End();
}

void FrameCaptionButtonContainerView::SetButtonImage(
    CaptionButtonIcon icon,
    const gfx::VectorIcon& icon_definition) {
  button_icon_map_[icon] = &icon_definition;

  FrameCaptionButton* buttons[] = {minimize_button_, size_button_,
                                   close_button_};
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    if (buttons[i]->icon() == icon)
      buttons[i]->SetImage(icon, FrameCaptionButton::ANIMATE_NO,
                           icon_definition);
  }
}

void FrameCaptionButtonContainerView::SetPaintAsActive(bool paint_as_active) {
  minimize_button_->set_paint_as_active(paint_as_active);
  size_button_->set_paint_as_active(paint_as_active);
  close_button_->set_paint_as_active(paint_as_active);
}

void FrameCaptionButtonContainerView::SetUseLightImages(bool light) {
  minimize_button_->set_use_light_images(light);
  size_button_->set_use_light_images(light);
  close_button_->set_use_light_images(light);
}

void FrameCaptionButtonContainerView::ResetWindowControls() {
  SetButtonsToNormal(ANIMATE_NO);
}

int FrameCaptionButtonContainerView::NonClientHitTest(
    const gfx::Point& point) const {
  if (close_button_->visible() &&
      ConvertPointToViewAndHitTest(this, close_button_, point)) {
    return HTCLOSE;
  } else if (size_button_->visible() &&
             ConvertPointToViewAndHitTest(this, size_button_, point)) {
    return HTMAXBUTTON;
  } else if (minimize_button_->visible() &&
             ConvertPointToViewAndHitTest(this, minimize_button_, point)) {
    return HTMINBUTTON;
  }
  return HTNOWHERE;
}

void FrameCaptionButtonContainerView::UpdateSizeButtonVisibility() {
  bool visible = ShouldSizeButtonBeVisible();
  if (visible) {
    size_button_->SetVisible(true);
    tablet_mode_animation_->SetSlideDuration(kShowAnimationDurationMs);
    tablet_mode_animation_->Show();
  } else {
    tablet_mode_animation_->SetSlideDuration(kHideAnimationDurationMs);
    tablet_mode_animation_->Hide();
  }
}

void FrameCaptionButtonContainerView::SetButtonSize(const gfx::Size& size) {
  minimize_button_->SetPreferredSize(size);
  size_button_->SetPreferredSize(size);
  close_button_->SetPreferredSize(size);
}

void FrameCaptionButtonContainerView::Layout() {
  views::View::Layout();
  if (tablet_mode_animation_->is_animating()) {
    AnimationProgressed(tablet_mode_animation_.get());
  }
}

const char* FrameCaptionButtonContainerView::GetClassName() const {
  return kViewClassName;
}

void FrameCaptionButtonContainerView::AnimationEnded(
    const gfx::Animation* animation) {
  // Ensure that position is calculated at least once.
  AnimationProgressed(animation);

  double current_value = tablet_mode_animation_->GetCurrentValue();
  if (current_value == 0.0) {
    size_button_->SetVisible(false);
    PreferredSizeChanged();
  }
}

void FrameCaptionButtonContainerView::AnimationProgressed(
    const gfx::Animation* animation) {
  double current_value = animation->GetCurrentValue();
  int size_alpha = 0;
  int minimize_x = 0;
  if (tablet_mode_animation_->IsShowing()) {
    double scaled_value =
        CapAnimationValue((current_value - SizeButtonShowStartValue()) /
                          SizeButtonShowDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 0, 255);

    double tweened_value_slide =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, current_value);
    minimize_x = gfx::Tween::LinearIntValueBetween(tweened_value_slide,
                                                   size_button_->x(), 0);
  } else {
    double scaled_value_alpha =
        CapAnimationValue((1.0f - current_value) / SizeButtonHideDuration());
    double tweened_value_alpha =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, scaled_value_alpha);
    size_alpha = gfx::Tween::LinearIntValueBetween(tweened_value_alpha, 255, 0);

    double scaled_value_position = CapAnimationValue(
        (HidePositionStartValue() - current_value) / HidePositionStartValue());
    double tweened_value_position =
        gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, scaled_value_position);
    minimize_x = gfx::Tween::LinearIntValueBetween(tweened_value_position, 0,
                                                   size_button_->x());
  }
  size_button_->SetAlpha(size_alpha);
  minimize_button_->SetX(minimize_x);
}

void FrameCaptionButtonContainerView::SetButtonIcon(FrameCaptionButton* button,
                                                    CaptionButtonIcon icon,
                                                    Animate animate) {
  // The early return is dependent on |animate| because callers use
  // SetButtonIcon() with ANIMATE_NO to progress |button|'s crossfade animation
  // to the end.
  if (button->icon() == icon &&
      (animate == ANIMATE_YES || !button->IsAnimatingImageSwap())) {
    return;
  }

  FrameCaptionButton::Animate fcb_animate =
      (animate == ANIMATE_YES) ? FrameCaptionButton::ANIMATE_YES
                               : FrameCaptionButton::ANIMATE_NO;
  auto it = button_icon_map_.find(icon);
  if (it != button_icon_map_.end())
    button->SetImage(icon, fcb_animate, *it->second);
}

bool FrameCaptionButtonContainerView::ShouldSizeButtonBeVisible() const {
  return !Shell::Get()
              ->tablet_mode_controller()
              ->IsTabletModeWindowManagerEnabled() &&
         frame_->widget_delegate()->CanMaximize();
}

void FrameCaptionButtonContainerView::ButtonPressed(views::Button* sender,
                                                    const ui::Event& event) {
  // Abort any animations of the button icons.
  SetButtonsToNormal(ANIMATE_NO);

  using base::RecordAction;
  using base::UserMetricsAction;
  if (sender == minimize_button_) {
    frame_->Minimize();
    RecordAction(UserMetricsAction("MinButton_Clk"));
  } else if (sender == size_button_) {
    if (frame_->IsFullscreen()) {  // Can be clicked in immersive fullscreen.
      frame_->Restore();
      RecordAction(UserMetricsAction("MaxButton_Clk_ExitFS"));
    } else if (frame_->IsMaximized()) {
      frame_->Restore();
      RecordAction(UserMetricsAction("MaxButton_Clk_Restore"));
    } else {
      frame_->Maximize();
      RecordAction(UserMetricsAction("MaxButton_Clk_Maximize"));
    }

    if (event.IsGestureEvent())
      TouchUMA::GetInstance()->RecordGestureAction(GESTURE_FRAMEMAXIMIZE_TAP);
  } else if (sender == close_button_) {
    frame_->Close();
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled()) {
      RecordAction(UserMetricsAction("Tablet_WindowCloseFromCaptionButton"));
    } else {
      RecordAction(UserMetricsAction("CloseButton_Clk"));
    }
  }
}

bool FrameCaptionButtonContainerView::IsMinimizeButtonVisible() const {
  return minimize_button_->visible();
}

void FrameCaptionButtonContainerView::SetButtonsToNormal(Animate animate) {
  SetButtonIcons(CAPTION_BUTTON_ICON_MINIMIZE, CAPTION_BUTTON_ICON_CLOSE,
                 animate);
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  size_button_->SetState(views::Button::STATE_NORMAL);
  close_button_->SetState(views::Button::STATE_NORMAL);
}

void FrameCaptionButtonContainerView::SetButtonIcons(
    CaptionButtonIcon minimize_button_icon,
    CaptionButtonIcon close_button_icon,
    Animate animate) {
  SetButtonIcon(minimize_button_, minimize_button_icon, animate);
  SetButtonIcon(close_button_, close_button_icon, animate);
}

const FrameCaptionButton* FrameCaptionButtonContainerView::GetButtonClosestTo(
    const gfx::Point& position_in_screen) const {
  // Since the buttons all have the same size, the closest button is the button
  // with the center point closest to |position_in_screen|.
  // TODO(pkotwicz): Make the caption buttons not overlap.
  gfx::Point position(position_in_screen);
  views::View::ConvertPointFromScreen(this, &position);

  FrameCaptionButton* buttons[] = {minimize_button_, size_button_,
                                   close_button_};
  int min_squared_distance = INT_MAX;
  FrameCaptionButton* closest_button = NULL;
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    FrameCaptionButton* button = buttons[i];
    if (!button->visible())
      continue;

    gfx::Point center_point = button->GetLocalBounds().CenterPoint();
    views::View::ConvertPointToTarget(button, this, &center_point);
    int squared_distance = static_cast<int>(
        pow(static_cast<double>(position.x() - center_point.x()), 2) +
        pow(static_cast<double>(position.y() - center_point.y()), 2));
    if (squared_distance < min_squared_distance) {
      min_squared_distance = squared_distance;
      closest_button = button;
    }
  }
  return closest_button;
}

void FrameCaptionButtonContainerView::SetHoveredAndPressedButtons(
    const FrameCaptionButton* to_hover,
    const FrameCaptionButton* to_press) {
  FrameCaptionButton* buttons[] = {minimize_button_, size_button_,
                                   close_button_};
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    FrameCaptionButton* button = buttons[i];
    views::Button::ButtonState new_state = views::Button::STATE_NORMAL;
    if (button == to_hover)
      new_state = views::Button::STATE_HOVERED;
    else if (button == to_press)
      new_state = views::Button::STATE_PRESSED;
    button->SetState(new_state);
  }
}

}  // namespace ash
