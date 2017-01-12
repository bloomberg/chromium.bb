// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/touch_calibrator/touch_calibrator_view.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

constexpr char kWidgetName[] = "TouchCalibratorOverlay";

constexpr int kAnimationFrameRate = 100;
constexpr int kFadeDurationInMs = 150;

const SkColor kExitLabelColor = SkColorSetARGBInline(255, 96, 96, 96);
const SkColor kExitLabelShadowColor = SkColorSetARGBInline(255, 11, 11, 11);
constexpr int kExitLabelWidth = 300;
constexpr int kExitLabelHeight = 20;

constexpr float kBackgroundFinalOpacity = 0.75f;

// Returns the initialization params for the widget that contains the touch
// calibrator view.
views::Widget::InitParams GetWidgetParams(aura::Window* root_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.name = kWidgetName;
  params.keep_on_top = true;
  params.accept_events = true;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent = ash::Shell::GetContainer(
      root_window, ash::kShellWindowId_OverlayContainer);
  return params;
}

}  // namespace

TouchCalibratorView::TouchCalibratorView(const display::Display& target_display,
                                         bool is_primary_view)
    : display_(target_display),
      is_primary_view_(is_primary_view),
      exit_label_(nullptr) {
  aura::Window* root = ash::Shell::GetInstance()
                           ->window_tree_host_manager()
                           ->GetRootWindowForDisplayId(display_.id());
  widget_.reset(new views::Widget);
  widget_->Init(GetWidgetParams(root));
  widget_->SetContentsView(this);
  widget_->SetBounds(display_.bounds());
  widget_->Show();
  set_owned_by_client();

  animator_.reset(
      new gfx::LinearAnimation(kFadeDurationInMs, kAnimationFrameRate, this));

  InitViewContents();
  AdvanceToNextState();
}

TouchCalibratorView::~TouchCalibratorView() {
  state_ = UNKNOWN;
  widget_->Hide();
  animator_->End();
}

void TouchCalibratorView::InitViewContents() {
  // Initialize the background rect.
  background_rect_ =
      gfx::RectF(0, 0, display_.bounds().width(), display_.bounds().height());

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // Initialize exit label that informs the user how to exit the touch
  // calibration setup.
  exit_label_ = new views::Label(
      rb.GetLocalizedString(IDS_DISPLAY_TOUCH_CALIBRATION_EXIT_LABEL),
      rb.GetFontListWithDelta(8, gfx::Font::FontStyle::NORMAL,
                              gfx::Font::Weight::NORMAL));
  exit_label_->SetBounds((display_.bounds().width() - kExitLabelWidth) / 2,
                         display_.bounds().height() * 3.f / 4, kExitLabelWidth,
                         kExitLabelHeight);
  exit_label_->SetEnabledColor(kExitLabelColor);
  exit_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  exit_label_->SetShadows(gfx::ShadowValues(
      1, gfx::ShadowValue(gfx::Vector2d(1, 1), 1, kExitLabelShadowColor)));
  exit_label_->SetSubpixelRenderingEnabled(false);
  exit_label_->SetVisible(false);

  AddChildView(exit_label_);
}

void TouchCalibratorView::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
}

void TouchCalibratorView::OnPaintBackground(gfx::Canvas* canvas) {
  float opacity;

  // If current state is a fade in or fade out state then update opacity
  // based on how far the animation has progressed.
  if (animator_ && (state_ == TouchCalibratorView::BACKGROUND_FADING_OUT ||
                    state_ == TouchCalibratorView::BACKGROUND_FADING_IN)) {
    opacity = static_cast<float>(animator_->CurrentValueBetween(
        start_opacity_value_, end_opacity_value_));
  } else {
    opacity = state_ == BACKGROUND_FADING_OUT ? 0.f : kBackgroundFinalOpacity;
  }

  paint_.setColor(SkColorSetA(SK_ColorBLACK,
                              std::numeric_limits<uint8_t>::max() * opacity));
  canvas->DrawRect(background_rect_, paint_);
}

void TouchCalibratorView::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void TouchCalibratorView::AnimationCanceled(const gfx::Animation* animation) {
  AnimationEnded(animation);
}

void TouchCalibratorView::AnimationEnded(const gfx::Animation* animation) {
  switch (state_) {
    case BACKGROUND_FADING_IN:
      exit_label_->SetVisible(true);
      state_ = is_primary_view_ ? DISPLAY_POINT_1 : CALIBRATION_COMPLETE;
      break;
    default:
      break;
  }
}

void TouchCalibratorView::AdvanceToNextState() {
  // Stop any previous animations and skip them to the end.
  animator_->End();

  switch (state_) {
    case UNKNOWN:
    case BACKGROUND_FADING_IN:
      state_ = BACKGROUND_FADING_IN;
      start_opacity_value_ = 0.f;
      end_opacity_value_ = kBackgroundFinalOpacity;

      paint_.setStyle(SkPaint::kFill_Style);

      animator_->SetDuration(kFadeDurationInMs);
      break;
    default:
      break;
  }
  animator_->Start();
}

bool TouchCalibratorView::GetDisplayPointLocation(gfx::Point* location) {
  if (!is_primary_view_)
    return false;
  return false;
}

void TouchCalibratorView::SkipToFinalState() {}

void TouchCalibratorView::SkipCurrentAnimationForTest() {
  if (animator_->is_animating())
    animator_->End();
}

}  // namespace chromeos
