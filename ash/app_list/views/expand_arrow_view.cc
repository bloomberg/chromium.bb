// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/expand_arrow_view.h"

#include <memory>
#include <utility>

#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

namespace app_list {

namespace {

// The width of this view.
constexpr int kTileWidth = 60;

// The arrow dimension of expand arrow.
constexpr int kArrowDimension = 12;

constexpr int kInkDropRadius = 18;
constexpr int kCircleRadius = 18;

// The y position of circle center in peeking and fullscreen state.
constexpr int kCircleCenterPeekingY = 42;
constexpr int kCircleCenterFullscreenY = 8;

// The arrow's y position in peeking and fullscreen state.
constexpr int kArrowPeekingY = 36;
constexpr int kArrowFullscreenY = 2;

// The stroke width of the arrow.
constexpr int kExpandArrowStrokeWidth = 2;

// The three points of arrow in peeking and fullscreen state.
constexpr size_t kPointCount = 3;
constexpr gfx::Point kPeekingPoints[] = {
    gfx::Point(0, kArrowDimension / 4 * 3),
    gfx::Point(kArrowDimension / 2, kArrowDimension / 4),
    gfx::Point(kArrowDimension, kArrowDimension / 4 * 3)};
constexpr gfx::Point kFullscreenPoints[] = {
    gfx::Point(0, kArrowDimension / 2),
    gfx::Point(kArrowDimension / 2, kArrowDimension / 2),
    gfx::Point(kArrowDimension, kArrowDimension / 2)};

// Arrow vertical transiton animation related constants.
constexpr int kTotalArrowYOffset = 24;
constexpr int kPulseMinRadius = 2;
constexpr int kPulseMaxRadius = 30;
constexpr float kPulseMinOpacity = 0.f;
constexpr float kPulseMaxOpacity = 0.3f;
constexpr int kAnimationInitialWaitTimeInSec = 3;
constexpr int kAnimationIntervalInSec = 10;
constexpr int kCycleDurationInMs = 1000;
constexpr int kCycleIntervalInMs = 500;
constexpr int kPulseOpacityShowBeginTimeInMs = 100;
constexpr int kPulseOpacityShowEndTimeInMs = 200;
constexpr int kPulseOpacityHideBeginTimeInMs = 800;
constexpr int kPulseOpacityHideEndTimeInMs = 1000;
constexpr int kArrowMoveOutBeginTimeInMs = 100;
constexpr int kArrowMoveOutEndTimeInMs = 500;
constexpr int kArrowMoveInBeginTimeInMs = 500;
constexpr int kArrowMoveInEndTimeInMs = 900;

constexpr SkColor kExpandArrowColor = SK_ColorWHITE;
constexpr SkColor kPulseColor = SK_ColorWHITE;
constexpr SkColor kUnFocusedBackgroundColor =
    SkColorSetARGB(0xF, 0xFF, 0xFF, 0xFF);
constexpr SkColor kFocusedBackgroundColor =
    SkColorSetARGB(0x3D, 0xFF, 0xFF, 0xFF);
constexpr SkColor kInkDropRippleColor = SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF);

}  // namespace

ExpandArrowView::ExpandArrowView(ContentsView* contents_view,
                                 AppListView* app_list_view)
    : views::Button(this),
      contents_view_(contents_view),
      app_list_view_(app_list_view),
      is_new_style_launcher_enabled_(features::IsNewStyleLauncherEnabled()),
      weak_ptr_factory_(this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  SetInkDropMode(InkDropHostView::InkDropMode::ON);

  SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_LIST_EXPAND_BUTTON));

  animation_.reset(new gfx::SlideAnimation(this));
  animation_->SetTweenType(gfx::Tween::LINEAR);
  animation_->SetSlideDuration(kCycleDurationInMs * 2 + kCycleIntervalInMs);
  ResetHintingAnimation();
  ScheduleHintingAnimation(true);
}

ExpandArrowView::~ExpandArrowView() = default;

void ExpandArrowView::PaintButtonContents(gfx::Canvas* canvas) {
  const int current_height = app_list_view_->GetCurrentAppListHeight();
  const int peeking_height =
      AppListConfig::instance().peeking_app_list_height();
  gfx::Point circle_center(kTileWidth / 2, kCircleCenterPeekingY);
  gfx::Point arrow_origin((kTileWidth - kArrowDimension) / 2, kArrowPeekingY);
  gfx::Point arrow_points[kPointCount];
  for (size_t i = 0; i < kPointCount; ++i)
    arrow_points[i] = kPeekingPoints[i];
  SkColor circle_color =
      HasFocus() ? kFocusedBackgroundColor : kUnFocusedBackgroundColor;

  if (current_height > peeking_height && is_new_style_launcher_enabled_) {
    // If app list is transitioning from peeking to fullscreen state, the arrow
    // and circle should move up, the arrow should transition to a horizontal
    // line and the circle should become transparent.
    const double peeking_to_fullscreen_height =
        contents_view_->GetDisplaySize().height() - peeking_height;
    DCHECK_GT(peeking_to_fullscreen_height, 0);
    const double progress =
        (current_height - peeking_height) / peeking_to_fullscreen_height;
    circle_center.set_y(gfx::Tween::IntValueBetween(
        progress, kCircleCenterPeekingY, kCircleCenterFullscreenY));
    arrow_origin.set_y(gfx::Tween::IntValueBetween(progress, kArrowPeekingY,
                                                   kArrowFullscreenY));
    for (size_t i = 0; i < kPointCount; ++i) {
      arrow_points[i].set_y(gfx::Tween::IntValueBetween(
          progress, kPeekingPoints[i].y(), kFullscreenPoints[i].y()));
    }
    circle_color = gfx::Tween::ColorValueBetween(progress, circle_color,
                                                 SK_ColorTRANSPARENT);
  } else if (animation_->is_animating()) {
    // If app list is peeking state or below peeking state, the arrow should
    // keep runing transition animation.
    arrow_origin.Offset(0, arrow_y_offset_);
  }

  // Draw a circle.
  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(circle_color);
  circle_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(circle_center, kCircleRadius, circle_flags);

  if (animation_->is_animating() && current_height <= peeking_height) {
    // Draw a pulse that expands around the circle.
    cc::PaintFlags pulse_flags;
    pulse_flags.setStyle(cc::PaintFlags::kStroke_Style);
    pulse_flags.setColor(
        SkColorSetA(kPulseColor, static_cast<U8CPU>(255 * pulse_opacity_)));
    pulse_flags.setAntiAlias(true);
    canvas->DrawCircle(circle_center, pulse_radius_, pulse_flags);
  }

  // Draw an arrow. (It becomes a horizontal line in fullscreen state.)
  for (auto& point : arrow_points)
    point.Offset(arrow_origin.x(), arrow_origin.y());

  cc::PaintFlags arrow_flags;
  arrow_flags.setAntiAlias(true);
  arrow_flags.setColor(kExpandArrowColor);
  arrow_flags.setStrokeWidth(kExpandArrowStrokeWidth);
  arrow_flags.setStyle(cc::PaintFlags::kStroke_Style);

  gfx::Path arrow_path;
  arrow_path.moveTo(arrow_points[0].x(), arrow_points[0].y());
  for (size_t i = 1; i < kPointCount; ++i)
    arrow_path.lineTo(arrow_points[i].x(), arrow_points[i].y());
  canvas->DrawPath(arrow_path, arrow_flags);
}

void ExpandArrowView::ButtonPressed(views::Button* /*sender*/,
                                    const ui::Event& /*event*/) {
  button_pressed_ = true;
  ResetHintingAnimation();
  TransitToFullscreenAllAppsState();
  GetInkDrop()->AnimateToState(views::InkDropState::ACTION_TRIGGERED);
}

gfx::Size ExpandArrowView::CalculatePreferredSize() const {
  return gfx::Size(kTileWidth,
                   AppListConfig::instance().expand_arrow_tile_height());
}

bool ExpandArrowView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() != ui::VKEY_RETURN)
    return false;
  TransitToFullscreenAllAppsState();
  return true;
}

void ExpandArrowView::OnFocus() {
  SchedulePaint();
  Button::OnFocus();
}

void ExpandArrowView::OnBlur() {
  SchedulePaint();
  Button::OnBlur();
}

std::unique_ptr<views::InkDrop> ExpandArrowView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropMask> ExpandArrowView::CreateInkDropMask() const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), gfx::Point(kTileWidth / 2, kCircleCenterPeekingY),
      kInkDropRadius);
}

std::unique_ptr<views::InkDropRipple> ExpandArrowView::CreateInkDropRipple()
    const {
  gfx::Point center(kTileWidth / 2, kCircleCenterPeekingY);
  gfx::Rect bounds(center.x() - kInkDropRadius, center.y() - kInkDropRadius,
                   2 * kInkDropRadius, 2 * kInkDropRadius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), kInkDropRippleColor, 1.0f);
}

void ExpandArrowView::AnimationProgressed(const gfx::Animation* animation) {
  // There are two cycles in one animation.
  const int animation_duration = kCycleDurationInMs * 2 + kCycleIntervalInMs;
  const int first_cycle_end_time = kCycleDurationInMs;
  const int interval_end_time = kCycleDurationInMs + kCycleIntervalInMs;
  const int second_cycle_end_time = kCycleDurationInMs * 2 + kCycleIntervalInMs;
  int time_in_ms = animation->GetCurrentValue() * animation_duration;

  if (time_in_ms > first_cycle_end_time && time_in_ms <= interval_end_time) {
    // There's no animation in the interval between cycles.
    return;
  } else if (time_in_ms > interval_end_time &&
             time_in_ms <= second_cycle_end_time) {
    // Convert to time in one single cycle.
    time_in_ms -= interval_end_time;
  }

  // Update pulse opacity.
  if (time_in_ms > kPulseOpacityShowBeginTimeInMs &&
      time_in_ms <= kPulseOpacityShowEndTimeInMs) {
    pulse_opacity_ =
        kPulseMinOpacity +
        (kPulseMaxOpacity - kPulseMinOpacity) *
            (time_in_ms - kPulseOpacityShowBeginTimeInMs) /
            (kPulseOpacityShowEndTimeInMs - kPulseOpacityShowBeginTimeInMs);
  } else if (time_in_ms > kPulseOpacityHideBeginTimeInMs &&
             time_in_ms <= kPulseOpacityHideEndTimeInMs) {
    pulse_opacity_ =
        kPulseMaxOpacity -
        (kPulseMaxOpacity - kPulseMinOpacity) *
            (time_in_ms - kPulseOpacityHideBeginTimeInMs) /
            (kPulseOpacityHideEndTimeInMs - kPulseOpacityHideBeginTimeInMs);
  }

  // Update pulse radius.
  pulse_radius_ = static_cast<int>(
      (kPulseMaxRadius - kPulseMinRadius) *
      gfx::Tween::CalculateValue(
          gfx::Tween::EASE_IN_OUT,
          static_cast<double>(time_in_ms) / kCycleDurationInMs));

  // Update y position offset of the arrow.
  if (time_in_ms > kArrowMoveOutBeginTimeInMs &&
      time_in_ms <= kArrowMoveOutEndTimeInMs) {
    const double progress =
        static_cast<double>(time_in_ms - kArrowMoveOutBeginTimeInMs) /
        (kArrowMoveOutEndTimeInMs - kArrowMoveOutBeginTimeInMs);
    arrow_y_offset_ = static_cast<int>(
        -kTotalArrowYOffset *
        gfx::Tween::CalculateValue(gfx::Tween::EASE_IN, progress));
  } else if (time_in_ms > kArrowMoveInBeginTimeInMs &&
             time_in_ms <= kArrowMoveInEndTimeInMs) {
    const double progress =
        static_cast<double>(time_in_ms - kArrowMoveInBeginTimeInMs) /
        (kArrowMoveInEndTimeInMs - kArrowMoveInBeginTimeInMs);
    arrow_y_offset_ = static_cast<int>(
        kTotalArrowYOffset *
        (1 - gfx::Tween::CalculateValue(gfx::Tween::EASE_OUT, progress)));
  }

  // Apply updates.
  SchedulePaint();
}

void ExpandArrowView::AnimationEnded(const gfx::Animation* /*animation*/) {
  ResetHintingAnimation();
  if (!button_pressed_)
    ScheduleHintingAnimation(false);
}

void ExpandArrowView::TransitToFullscreenAllAppsState() {
  UMA_HISTOGRAM_ENUMERATION(kPageOpenedHistogram, ash::AppListState::kStateApps,
                            ash::AppListState::kStateLast);
  UMA_HISTOGRAM_ENUMERATION(kAppListPeekingToFullscreenHistogram, kExpandArrow,
                            kMaxPeekingToFullscreen);
  contents_view_->SetActiveState(ash::AppListState::kStateApps);
  app_list_view_->SetState(AppListViewState::FULLSCREEN_ALL_APPS);
}

void ExpandArrowView::ScheduleHintingAnimation(bool is_first_time) {
  int delay_in_sec = kAnimationIntervalInSec;
  if (is_first_time)
    delay_in_sec = kAnimationInitialWaitTimeInSec;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ExpandArrowView::StartHintingAnimation,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(delay_in_sec));
}

void ExpandArrowView::StartHintingAnimation() {
  if (!button_pressed_)
    animation_->Show();
}

void ExpandArrowView::ResetHintingAnimation() {
  pulse_opacity_ = kPulseMinOpacity;
  pulse_radius_ = kPulseMinRadius;
  animation_->Reset();
  Layout();
}

}  // namespace app_list
