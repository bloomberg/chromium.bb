// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/aura/gesture_nav_simple.h"

#include <utility>

#include "base/macros.h"
#include "cc/paint/paint_flags.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/overscroll_configuration.h"
#include "third_party/skia/include/core/SkDrawLooper.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/vector_icons/vector_icons.h"

namespace content {

namespace {

// Parameters defining the arrow icon inside the affordance.
const int kArrowSize = 16;
const SkColor kArrowColor = gfx::kGoogleBlue500;

// Parameters defining the background circle of the affordance.
const int kBackgroundRadius = 18;
const SkColor kBackgroundColor = SK_ColorWHITE;
const int kBgShadowOffsetY = 2;
const int kBgShadowBlurRadius = 8;
const SkColor kBgShadowColor = SkColorSetA(SK_ColorBLACK, 0x4D);

// Parameters defining the affordance ripple. The ripple fades in and grows as
// the user drags the affordance until it reaches |kMaxRippleRadius|. If the
// overscroll is successful, the ripple will burst by fading out and growing to
// |kMaxRippleBurstRadius|.
const int kMaxRippleRadius = 54;
const SkColor kRippleColor = SkColorSetA(gfx::kGoogleBlue500, 0x33);
const int kMaxRippleBurstRadius = 72;
const gfx::Tween::Type kBurstAnimationTweenType = gfx::Tween::EASE_IN;
const int kRippleBurstAnimationDuration = 160;

// Offset of the affordance when it is at the maximum distance with content
// border. Since the affordance is initially out of content bounds, this is the
// offset of the farther side of the affordance (which equals 128 + 18).
const int kMaxAffordanceOffset = 146;

// Parameters defining animation when the affordance is aborted.
const gfx::Tween::Type kAbortAnimationTweenType = gfx::Tween::EASE_IN;
const int kAbortAnimationDuration = 300;

bool ShouldNavigateForward(const NavigationController& controller,
                           OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST) &&
         controller.CanGoForward();
}

bool ShouldNavigateBack(const NavigationController& controller,
                        OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST) &&
         controller.CanGoBack();
}

}  // namespace

// This class is responsible for creating, painting, and positioning the layer
// for the gesture nav affordance.
class GestureNavSimple::Affordance : public ui::LayerDelegate,
                                     public gfx::AnimationDelegate {
 public:
  Affordance(OverscrollMode mode, const gfx::Rect& content_bounds);
  ~Affordance() override;

  // Sets progress of affordance drag as a value between 0 and 1.
  void SetDragProgress(float progress);

  // Aborts the affordance and animates it back. This will delete |this|
  // instance after the animation.
  void Abort();

  // Completes the affordance by doing a ripple burst animation. This will
  // delete |this| instance after the animation.
  void Complete();

  // Returns the root layer of the affordance.
  ui::Layer* root_layer() const { return root_layer_.get(); }

 private:
  enum class State { DRAGGING, ABORTING, COMPLETING };

  void UpdateTransform();
  void SchedulePaint();
  void SetAbortProgress(float progress);
  void SetCompleteProgress(float progress);

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  const OverscrollMode mode_;

  // Root layer of the affordance. This is used to clip the affordance to the
  // content bounds.
  std::unique_ptr<ui::Layer> root_layer_;

  // Layer that actually paints the affordance.
  std::unique_ptr<ui::Layer> painted_layer_;

  // Arrow image to be used for the affordance.
  const gfx::Image image_;

  // Values that determine current state of the affordance.
  State state_ = State::DRAGGING;
  float drag_progress_ = 0.f;
  float abort_progress_ = 0.f;
  float complete_progress_ = 0.f;

  std::unique_ptr<gfx::LinearAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(Affordance);
};

GestureNavSimple::Affordance::Affordance(OverscrollMode mode,
                                         const gfx::Rect& content_bounds)
    : mode_(mode),
      root_layer_(base::MakeUnique<ui::Layer>(ui::LAYER_NOT_DRAWN)),
      painted_layer_(base::MakeUnique<ui::Layer>(ui::LAYER_TEXTURED)),
      image_(gfx::CreateVectorIcon(
          mode == OVERSCROLL_EAST ? ui::kBackArrowIcon : ui::kForwardArrowIcon,
          kArrowSize,
          kArrowColor)) {
  DCHECK(mode == OVERSCROLL_EAST || mode == OVERSCROLL_WEST);
  DCHECK(!image_.IsEmpty());

  root_layer_->SetBounds(content_bounds);
  root_layer_->SetMasksToBounds(true);

  painted_layer_->SetFillsBoundsOpaquely(false);
  int x =
      mode_ == OVERSCROLL_EAST
          ? -kMaxRippleBurstRadius - kBackgroundRadius
          : content_bounds.width() - kMaxRippleBurstRadius + kBackgroundRadius;
  int y = std::max(0, content_bounds.height() / 2 - kMaxRippleBurstRadius);
  painted_layer_->SetBounds(
      gfx::Rect(x, y, 2 * kMaxRippleBurstRadius, 2 * kMaxRippleBurstRadius));
  painted_layer_->set_delegate(this);

  root_layer_->Add(painted_layer_.get());
}

GestureNavSimple::Affordance::~Affordance() {}

void GestureNavSimple::Affordance::SetDragProgress(float progress) {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (drag_progress_ == progress)
    return;
  drag_progress_ = progress;

  UpdateTransform();
  SchedulePaint();
}

void GestureNavSimple::Affordance::Abort() {
  DCHECK_EQ(State::DRAGGING, state_);

  state_ = State::ABORTING;

  animation_.reset(
      new gfx::LinearAnimation(drag_progress_ * kAbortAnimationDuration,
                               gfx::LinearAnimation::kDefaultFrameRate, this));
  animation_->Start();
}

void GestureNavSimple::Affordance::Complete() {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_EQ(1.f, drag_progress_);

  state_ = State::COMPLETING;

  animation_.reset(
      new gfx::LinearAnimation(kRippleBurstAnimationDuration,
                               gfx::LinearAnimation::kDefaultFrameRate, this));
  animation_->Start();
}

void GestureNavSimple::Affordance::UpdateTransform() {
  float offset = (1 - abort_progress_) * drag_progress_ * kMaxAffordanceOffset;
  gfx::Transform transform;
  transform.Translate(mode_ == OVERSCROLL_EAST ? offset : -offset, 0);
  painted_layer_->SetTransform(transform);
}

void GestureNavSimple::Affordance::SchedulePaint() {
  painted_layer_->SchedulePaint(gfx::Rect(painted_layer_->size()));
}

void GestureNavSimple::Affordance::SetAbortProgress(float progress) {
  DCHECK_EQ(State::ABORTING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (abort_progress_ == progress)
    return;
  abort_progress_ = progress;

  UpdateTransform();
  SchedulePaint();
}

void GestureNavSimple::Affordance::SetCompleteProgress(float progress) {
  DCHECK_EQ(State::COMPLETING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (complete_progress_ == progress)
    return;
  complete_progress_ = progress;

  painted_layer_->SetOpacity(1 - complete_progress_);
  SchedulePaint();
}

void GestureNavSimple::Affordance::OnPaintLayer(
    const ui::PaintContext& context) {
  DCHECK(drag_progress_ == 1.f || state_ != State::COMPLETING);
  DCHECK(abort_progress_ == 0.f || state_ == State::ABORTING);
  DCHECK(complete_progress_ == 0.f || state_ == State::COMPLETING);

  ui::PaintRecorder recorder(context, painted_layer_->size());
  gfx::Canvas* canvas = recorder.canvas();

  gfx::PointF center_point(kMaxRippleBurstRadius, kMaxRippleBurstRadius);
  float progress = (1 - abort_progress_) * drag_progress_;

  // Draw the ripple.
  cc::PaintFlags ripple_flags;
  ripple_flags.setAntiAlias(true);
  ripple_flags.setStyle(cc::PaintFlags::kFill_Style);
  ripple_flags.setColor(kRippleColor);
  float ripple_radius;
  if (state_ == State::COMPLETING) {
    ripple_radius =
        kMaxRippleRadius +
        complete_progress_ * (kMaxRippleBurstRadius - kMaxRippleRadius);
  } else {
    ripple_radius =
        kBackgroundRadius + progress * (kMaxRippleRadius - kBackgroundRadius);
  }
  canvas->DrawCircle(center_point, ripple_radius, ripple_flags);

  // Draw the arrow background circle with the shadow.
  cc::PaintFlags bg_flags;
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);
  bg_flags.setColor(kBackgroundColor);
  gfx::ShadowValues shadow;
  shadow.emplace_back(gfx::Vector2d(0, kBgShadowOffsetY), kBgShadowBlurRadius,
                      kBgShadowColor);
  bg_flags.setLooper(gfx::CreateShadowDrawLooperCorrectBlur(shadow));
  canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

  // Draw the arrow.
  float arrow_x = center_point.x() - kArrowSize / 2.f;
  float arrow_y = center_point.y() - kArrowSize / 2.f;
  // Calculate the offset for the arrow relative to its circular background.
  float arrow_x_offset =
      (1 - progress) * (-kBackgroundRadius + kArrowSize / 2.f);
  arrow_x += mode_ == OVERSCROLL_EAST ? arrow_x_offset : -arrow_x_offset;
  uint8_t arrow_alpha =
      static_cast<uint8_t>(std::min(0xFF, static_cast<int>(progress * 0xFF)));
  canvas->DrawImageInt(*image_.ToImageSkia(), static_cast<int>(arrow_x),
                       static_cast<int>(arrow_y), arrow_alpha);
}

void GestureNavSimple::Affordance::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {}

void GestureNavSimple::Affordance::OnDeviceScaleFactorChanged(
    float device_scale_factor) {}

void GestureNavSimple::Affordance::AnimationEnded(
    const gfx::Animation* animation) {
  delete this;
}

void GestureNavSimple::Affordance::AnimationProgressed(
    const gfx::Animation* animation) {
  switch (state_) {
    case State::DRAGGING:
      NOTREACHED();
      break;
    case State::ABORTING:
      SetAbortProgress(gfx::Tween::CalculateValue(
          kAbortAnimationTweenType, animation->GetCurrentValue()));
      break;
    case State::COMPLETING:
      SetCompleteProgress(gfx::Tween::CalculateValue(
          kBurstAnimationTweenType, animation->GetCurrentValue()));
      break;
  }
}

void GestureNavSimple::Affordance::AnimationCanceled(
    const gfx::Animation* animation) {
  NOTREACHED();
}


GestureNavSimple::GestureNavSimple(WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      completion_threshold_(0.f) {}

GestureNavSimple::~GestureNavSimple() {}

void GestureNavSimple::AbortGestureAnimation() {
  if (!affordance_)
    return;
  // Release the unique pointer. The affordance will delete itself upon
  // completion of animation.
  Affordance* affordance = affordance_.release();
  affordance->Abort();
}

void GestureNavSimple::CompleteGestureAnimation() {
  if (!affordance_)
    return;
  // Release the unique pointer. The affordance will delete itself upon
  // completion of animation.
  Affordance* affordance = affordance_.release();
  affordance->Complete();
}

gfx::Rect GestureNavSimple::GetVisibleBounds() const {
  return web_contents_->GetNativeView()->bounds();
}

bool GestureNavSimple::OnOverscrollUpdate(float delta_x, float delta_y) {
  if (!affordance_)
    return false;
  affordance_->SetDragProgress(
      std::min(1.f, std::abs(delta_x) / completion_threshold_));
  return true;
}

void GestureNavSimple::OnOverscrollComplete(OverscrollMode overscroll_mode) {
  CompleteGestureAnimation();

  NavigationControllerImpl& controller = web_contents_->GetController();
  if (ShouldNavigateForward(controller, overscroll_mode))
    controller.GoForward();
  else if (ShouldNavigateBack(controller, overscroll_mode))
    controller.GoBack();
}

void GestureNavSimple::OnOverscrollModeChange(OverscrollMode old_mode,
                                              OverscrollMode new_mode,
                                              OverscrollSource source) {
  NavigationControllerImpl& controller = web_contents_->GetController();
  if (!ShouldNavigateForward(controller, new_mode) &&
      !ShouldNavigateBack(controller, new_mode)) {
    AbortGestureAnimation();
    return;
  }

  aura::Window* window = web_contents_->GetNativeView();
  const gfx::Rect& window_bounds = window->bounds();
  DCHECK_NE(source, OverscrollSource::NONE);
  float start_threshold = GetOverscrollConfig(
      source == OverscrollSource::TOUCHPAD
          ? OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD
          : OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN);
  completion_threshold_ =
      window_bounds.width() *
          GetOverscrollConfig(OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE) -
      start_threshold;

  affordance_.reset(new Affordance(new_mode, window_bounds));

  // Adding the affordance as a child of the content window is not sufficient,
  // because it is possible for a new layer to be parented on top of the
  // affordance layer (e.g. when the navigated-to page is displayed while the
  // completion animation is in progress). So instead, it is installed on top of
  // the content window as its sibling. Note that the affordance itself makes
  // sure that its contents are clipped to the bounds given to it.
  ui::Layer* parent = window->layer()->parent();
  parent->Add(affordance_->root_layer());
  parent->StackAtTop(affordance_->root_layer());
}

}  // namespace content
