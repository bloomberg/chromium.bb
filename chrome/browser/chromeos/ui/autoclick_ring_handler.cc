// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/autoclick_ring_handler.h"

#include <memory>

#include "ash/aura/wm_window_aura.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm/root_window_finder.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/transform.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace chromeos {
namespace {

const int kAutoclickRingOuterRadius = 30;
const int kAutoclickRingInnerRadius = 20;

// Angles from x-axis at which the outer and inner circles start.
const int kAutoclickRingInnerStartAngle = -90;

const int kAutoclickRingGlowWidth = 20;
// The following is half width to avoid division by 2.
const int kAutoclickRingArcWidth = 2;

// Start and end values for various animations.
const double kAutoclickRingScaleStartValue = 1.0;
const double kAutoclickRingScaleEndValue = 1.0;
const double kAutoclickRingShrinkScaleEndValue = 0.5;

const double kAutoclickRingOpacityStartValue = 0.1;
const double kAutoclickRingOpacityEndValue = 0.5;
const int kAutoclickRingAngleStartValue = -90;
// The sweep angle is a bit greater than 360 to make sure the circle
// completes at the end of the animation.
const int kAutoclickRingAngleEndValue = 360;

// Visual constants.
const SkColor kAutoclickRingArcColor = SkColorSetARGB(255, 0, 255, 0);
const SkColor kAutoclickRingCircleColor = SkColorSetARGB(255, 0, 0, 255);
const int kAutoclickRingFrameRateHz = 60;

views::Widget* CreateAutoclickRingWidget(aura::Window* root_window) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = root_window;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = ash::Shell::GetContainer(
      root_window, ash::kShellWindowId_OverlayContainer);

  widget->Init(params);
  widget->SetOpacity(1.f);
  return widget;
}

void PaintAutoclickRingCircle(gfx::Canvas* canvas,
                              gfx::Point& center,
                              int radius) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2 * kAutoclickRingArcWidth);
  paint.setColor(kAutoclickRingCircleColor);
  paint.setAntiAlias(true);

  canvas->DrawCircle(center, radius, paint);
}

void PaintAutoclickRingArc(gfx::Canvas* canvas,
                           const gfx::Point& center,
                           int radius,
                           int start_angle,
                           int end_angle) {
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2 * kAutoclickRingArcWidth);
  paint.setColor(kAutoclickRingArcColor);
  paint.setAntiAlias(true);

  SkPath arc_path;
  arc_path.addArc(SkRect::MakeXYWH(center.x() - radius, center.y() - radius,
                                   2 * radius, 2 * radius),
                  start_angle, end_angle - start_angle);
  canvas->DrawPath(arc_path, paint);
}
}  // namespace

// View of the AutoclickRingHandler. Draws the actual contents and updates as
// the animation proceeds. It also maintains the views::Widget that the
// animation is shown in.
class AutoclickRingHandler::AutoclickRingView : public views::View {
 public:
  AutoclickRingView(const gfx::Point& event_location, aura::Window* root_window)
      : views::View(),
        widget_(CreateAutoclickRingWidget(root_window)),
        current_angle_(kAutoclickRingAngleStartValue),
        current_scale_(kAutoclickRingScaleStartValue) {
    widget_->SetContentsView(this);

    // We are owned by the AutoclickRingHandler.
    set_owned_by_client();
    SetNewLocation(event_location, root_window);
  }

  ~AutoclickRingView() override {}

  void SetNewLocation(const gfx::Point& new_event_location,
                      aura::Window* root_window) {
    gfx::Point point = new_event_location;
    widget_->SetBounds(gfx::Rect(
        point.x() - (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
        point.y() - (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
        GetPreferredSize().width(), GetPreferredSize().height()));
    widget_->Show();
    widget_->GetNativeView()->layer()->SetOpacity(
        kAutoclickRingOpacityStartValue);
  }

  void UpdateWithGrowAnimation(gfx::Animation* animation) {
    // Update the portion of the circle filled so far and re-draw.
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    current_scale_ = animation->CurrentValueBetween(
        kAutoclickRingScaleStartValue, kAutoclickRingScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                       kAutoclickRingOpacityEndValue));
    SchedulePaint();
  }

  void UpdateWithShrinkAnimation(gfx::Animation* animation) {
    current_angle_ = animation->CurrentValueBetween(
        kAutoclickRingInnerStartAngle, kAutoclickRingAngleEndValue);
    current_scale_ = animation->CurrentValueBetween(
        kAutoclickRingScaleEndValue, kAutoclickRingShrinkScaleEndValue);
    widget_->GetNativeView()->layer()->SetOpacity(
        animation->CurrentValueBetween(kAutoclickRingOpacityStartValue,
                                       kAutoclickRingOpacityEndValue));
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override {
    return gfx::Size(2 * (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth),
                     2 * (kAutoclickRingOuterRadius + kAutoclickRingGlowWidth));
  }

  void OnPaint(gfx::Canvas* canvas) override {
    gfx::Point center(GetPreferredSize().width() / 2,
                      GetPreferredSize().height() / 2);
    canvas->Save();

    gfx::Transform scale;
    scale.Scale(current_scale_, current_scale_);
    // We want to scale from the center.
    canvas->Translate(center.OffsetFromOrigin());
    canvas->Transform(scale);
    canvas->Translate(-center.OffsetFromOrigin());

    // Paint inner circle.
    PaintAutoclickRingArc(canvas, center, kAutoclickRingInnerRadius,
                          kAutoclickRingInnerStartAngle, current_angle_);
    // Paint outer circle.
    PaintAutoclickRingCircle(canvas, center, kAutoclickRingOuterRadius);

    canvas->Restore();
  }

  std::unique_ptr<views::Widget> widget_;
  int current_angle_;
  double current_scale_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickRingView);
};

////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, public
AutoclickRingHandler::AutoclickRingHandler()
    : gfx::LinearAnimation(kAutoclickRingFrameRateHz, nullptr),
      tap_down_target_(nullptr),
      current_animation_type_(AnimationType::NONE) {}

AutoclickRingHandler::~AutoclickRingHandler() {
  StopAutoclickRing();
}

void AutoclickRingHandler::StartGesture(
    base::TimeDelta duration,
    const gfx::Point& center_point_in_screen) {
  aura::Window* target = GetTargetWindow();
  if (tap_down_target_ && tap_down_target_ != target)
    return;
  StopAutoclickRing();
  SetTapDownTarget();
  tap_down_location_ = center_point_in_screen;
  current_animation_type_ = AnimationType::GROW_ANIMATION;
  animation_duration_ = duration;
  StartAnimation(base::TimeDelta());
}

void AutoclickRingHandler::StopGesture() {
  aura::Window* target = GetTargetWindow();
  if (tap_down_target_ && tap_down_target_ != target)
    return;
  StopAutoclickRing();
}

void AutoclickRingHandler::SetGestureCenter(
    const gfx::Point& center_point_in_screen) {
  tap_down_location_ = center_point_in_screen;
}
////////////////////////////////////////////////////////////////////////////////

// AutoclickRingHandler, private
aura::Window* AutoclickRingHandler::GetTargetWindow() {
  aura::Window* target = ash::WmWindowAura::GetAuraWindow(
      ash::wm::GetRootWindowAt(tap_down_location_));
  DCHECK(target) << "Root window not found while rendering autoclick circle;";
  return target;
}

void AutoclickRingHandler::SetTapDownTarget() {
  aura::Window* target = GetTargetWindow();
  SetTapDownTarget(target);
}

void AutoclickRingHandler::StartAnimation(base::TimeDelta delay) {
  int delay_ms = int{delay.InMilliseconds()};
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION: {
      aura::Window* root_window = tap_down_target_->GetRootWindow();
      view_.reset(new AutoclickRingView(tap_down_location_, root_window));
      SetDuration(delay_ms);
      Start();
      break;
    }
    case AnimationType::SHRINK_ANIMATION: {
      aura::Window* root_window = tap_down_target_->GetRootWindow();
      view_.reset(new AutoclickRingView(tap_down_location_, root_window));
      SetDuration(delay_ms);
      Start();
      break;
    }
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::StopAutoclickRing() {
  // Since, Animation::Stop() calls AnimationStopped(), we need to reset the
  // |current_animation_type_| before Stop(), otherwise AnimationStopped() may
  // start the timer again.
  current_animation_type_ = AnimationType::NONE;
  Stop();
  view_.reset();
  SetTapDownTarget(nullptr);
}

void AutoclickRingHandler::SetTapDownTarget(aura::Window* target) {
  if (tap_down_target_ == target)
    return;

  if (tap_down_target_)
    tap_down_target_->RemoveObserver(this);
  tap_down_target_ = target;
  if (tap_down_target_)
    tap_down_target_->AddObserver(this);
}

void AutoclickRingHandler::AnimateToState(double state) {
  DCHECK(view_.get());
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      view_->SetNewLocation(tap_down_location_, GetTargetWindow());
      view_->UpdateWithGrowAnimation(this);
      break;
    case AnimationType::SHRINK_ANIMATION:
      view_->SetNewLocation(tap_down_location_, GetTargetWindow());
      view_->UpdateWithShrinkAnimation(this);
      break;
    case AnimationType::NONE:
      NOTREACHED();
      break;
  }
}

void AutoclickRingHandler::AnimationStopped() {
  switch (current_animation_type_) {
    case AnimationType::GROW_ANIMATION:
      current_animation_type_ = AnimationType::SHRINK_ANIMATION;
      StartAnimation(animation_duration_);
      break;
    case AnimationType::SHRINK_ANIMATION:
      current_animation_type_ = AnimationType::NONE;
      break;
    case AnimationType::NONE:
      // fall through to reset the view.
      view_.reset();
      SetTapDownTarget(nullptr);
      break;
  }
}

void AutoclickRingHandler::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(tap_down_target_, window);
  StopAutoclickRing();
}

}  // namespace chromeos
