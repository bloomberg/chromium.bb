// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/voice_interaction_overlay.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/app_list_button.h"
#include "ash/shelf/ink_drop_button_listener.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chromeos/chromeos_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/paint_context.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
constexpr int kFullExpandDurationMs = 650;
constexpr int kFullRetractDurationMs = 300;
constexpr int kFullBurstDurationMs = 200;

constexpr float kRippleCircleInitRadiusDip = 40.f;
constexpr float kRippleCircleStartRadiusDip = 1.f;
constexpr float kRippleCircleRadiusDip = 77.f;
constexpr float kRippleCircleBurstRadiusDip = 96.f;
constexpr SkColor kRippleColor = SK_ColorWHITE;
constexpr int kRippleExpandDurationMs = 400;
constexpr int kRippleOpacityDurationMs = 100;
constexpr int kRippleOpacityRetractDurationMs = 200;
constexpr float kRippleOpacity = 0.2f;

constexpr float kIconInitSizeDip = 24.f;
constexpr float kIconStartSizeDip = 4.f;
constexpr float kIconSizeDip = 24.f;
constexpr float kIconOffsetDip = 56.f;
constexpr float kIconOpacity = 1.f;
constexpr int kIconBurstDurationMs = 100;

constexpr float kBackgroundInitSizeDip = 48.f;
constexpr float kBackgroundStartSizeDip = 10.f;
constexpr float kBackgroundSizeDip = 48.f;
constexpr int kBackgroundStartDelayMs = 100;
constexpr int kBackgroundOpacityDurationMs = 200;
constexpr float kBackgroundShadowElevationDip = 24.f;

class VoiceInteractionIconBackground : public ui::Layer,
                                       public ui::LayerDelegate {
 public:
  VoiceInteractionIconBackground() : Layer(ui::LAYER_NOT_DRAWN) {
    set_name("VoiceInteractionOverlay:BACKGROUND_LAYER");
    SetBounds(gfx::Rect(0, 0, kBackgroundInitSizeDip, kBackgroundInitSizeDip));
    SetFillsBoundsOpaquely(false);
    SetMasksToBounds(false);

    shadow_values_ =
        gfx::ShadowValue::MakeMdShadowValues(kBackgroundShadowElevationDip);
    const gfx::Insets shadow_margin =
        gfx::ShadowValue::GetMargin(shadow_values_);

    shadow_layer_.reset(new ui::Layer());
    shadow_layer_->set_delegate(this);
    shadow_layer_->SetFillsBoundsOpaquely(false);
    shadow_layer_->SetBounds(
        gfx::Rect(shadow_margin.left(), shadow_margin.top(),
                  kBackgroundInitSizeDip - shadow_margin.width(),
                  kBackgroundInitSizeDip - shadow_margin.height()));
    Add(shadow_layer_.get());
  }
  ~VoiceInteractionIconBackground() override{};

 private:
  void OnPaintLayer(const ui::PaintContext& context) override {
    // Radius is based on the parent layer size, the shadow layer is expanded
    // to make room for the shadow.
    float radius = size().width() / 2.f;

    ui::PaintRecorder recorder(context, shadow_layer_->size());
    gfx::Canvas* canvas = recorder.canvas();

    cc::PaintFlags flags;
    flags.setColor(SK_ColorWHITE);
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values_));
    gfx::Rect shadow_bounds = shadow_layer_->bounds();
    canvas->DrawCircle({radius - shadow_bounds.x(), radius - shadow_bounds.y()},
                       radius, flags);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  gfx::ShadowValues shadow_values_;

  std::unique_ptr<ui::Layer> shadow_layer_;

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionIconBackground);
};

class VoiceInteractionIcon : public ui::Layer, public ui::LayerDelegate {
 public:
  VoiceInteractionIcon() {
    set_name("VoiceInteractionOverlay:ICON_LAYER");
    SetBounds(gfx::Rect(0, 0, kIconInitSizeDip, kIconInitSizeDip));
    SetFillsBoundsOpaquely(false);
    SetMasksToBounds(false);
    set_delegate(this);
  }

 private:
  void OnPaintLayer(const ui::PaintContext& context) override {
    ui::PaintRecorder recorder(context, size());
    gfx::PaintVectorIcon(recorder.canvas(), kShelfVoiceInteractionIcon,
                         kIconInitSizeDip, 0);
  }

  void OnDelegatedFrameDamage(const gfx::Rect& damage_rect_in_dip) override {}

  void OnDeviceScaleFactorChanged(float device_scale_factor) override {}

  DISALLOW_COPY_AND_ASSIGN(VoiceInteractionIcon);
};
}  // namespace

VoiceInteractionOverlay::VoiceInteractionOverlay(AppListButton* host_view)
    : ripple_layer_(new ui::Layer()),
      icon_layer_(new VoiceInteractionIcon()),
      background_layer_(new VoiceInteractionIconBackground()),
      host_view_(host_view),
      is_bursting_(false),
      circle_layer_delegate_(kRippleColor, kRippleCircleInitRadiusDip) {
  SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  layer()->set_name("VoiceInteractionOverlay:ROOT_LAYER");
  layer()->SetMasksToBounds(false);

  ripple_layer_->SetBounds(gfx::Rect(0, 0, kRippleCircleInitRadiusDip * 2,
                                     kRippleCircleInitRadiusDip * 2));
  ripple_layer_->set_delegate(&circle_layer_delegate_);
  ripple_layer_->SetFillsBoundsOpaquely(false);
  ripple_layer_->SetMasksToBounds(true);
  ripple_layer_->set_name("VoiceInteractionOverlay:PAINTED_LAYER");
  layer()->Add(ripple_layer_.get());

  layer()->Add(background_layer_.get());

  layer()->Add(icon_layer_.get());
}

VoiceInteractionOverlay::~VoiceInteractionOverlay() {}

void VoiceInteractionOverlay::StartAnimation() {
  is_bursting_ = false;
  SetVisible(true);

  // Setup ripple initial state.
  ripple_layer_->SetOpacity(0);

  SkMScalar scale_factor =
      kRippleCircleStartRadiusDip / kRippleCircleInitRadiusDip;
  gfx::Transform transform;

  const gfx::Point center = host_view_->GetCenterPoint();
  transform.Translate(center.x() - kRippleCircleStartRadiusDip,
                      center.y() - kRippleCircleStartRadiusDip);
  transform.Scale(scale_factor, scale_factor);
  ripple_layer_->SetTransform(transform);

  // Setup ripple animations.
  {
    scale_factor = kRippleCircleRadiusDip / kRippleCircleInitRadiusDip;
    transform.MakeIdentity();
    transform.Translate(center.x() - kRippleCircleRadiusDip,
                        center.y() - kRippleCircleRadiusDip);
    transform.Scale(scale_factor, scale_factor);

    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kRippleExpandDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    ripple_layer_->SetTransform(transform);

    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kRippleOpacityDurationMs));
    ripple_layer_->SetOpacity(kRippleOpacity);
  }

  // We need to determine the animation direction based on shelf alignment.
  ShelfAlignment alignment =
      Shelf::ForWindow(host_view_->GetWidget()->GetNativeWindow())->alignment();

  // Setup icon initial state.
  icon_layer_->SetOpacity(0);
  transform.MakeIdentity();

  transform.Translate(center.x() - kIconStartSizeDip / 2.f,
                      center.y() - kIconStartSizeDip / 2.f);

  scale_factor = kIconStartSizeDip / kIconInitSizeDip;
  transform.Scale(scale_factor, scale_factor);
  icon_layer_->SetTransform(transform);

  // Setup icon animation.
  scale_factor = kIconSizeDip / kIconInitSizeDip;
  transform.MakeIdentity();
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    transform.Translate(center.x() - kIconSizeDip / 2 + kIconOffsetDip,
                        center.y() - kIconSizeDip / 2 - kIconOffsetDip);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    transform.Translate(center.x() - kIconSizeDip / 2 - kIconOffsetDip,
                        center.y() - kIconSizeDip / 2 + kIconOffsetDip);
  } else {
    DCHECK_EQ(alignment, SHELF_ALIGNMENT_LEFT);
    transform.Translate(center.x() - kIconSizeDip / 2 + kIconOffsetDip,
                        center.y() - kIconSizeDip / 2 + kIconOffsetDip);
  }
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(icon_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullExpandDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    icon_layer_->SetTransform(transform);
    icon_layer_->SetOpacity(kIconOpacity);
  }

  // Setup background initial state.
  background_layer_->SetVisible(false);
  background_layer_->SetOpacity(0);

  transform.MakeIdentity();
  transform.Translate(center.x() - kBackgroundStartSizeDip / 2.f,
                      center.y() - kBackgroundStartSizeDip / 2.f);

  scale_factor = kBackgroundStartSizeDip / kBackgroundInitSizeDip;
  transform.Scale(scale_factor, scale_factor);
  background_layer_->SetTransform(transform);

  // Setup background animation.
  scale_factor = kBackgroundSizeDip / kBackgroundInitSizeDip;
  transform.MakeIdentity();
  if (alignment == SHELF_ALIGNMENT_BOTTOM ||
      alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED) {
    transform.Translate(center.x() - kBackgroundSizeDip / 2 + kIconOffsetDip,
                        center.y() - kBackgroundSizeDip / 2 - kIconOffsetDip);
  } else if (alignment == SHELF_ALIGNMENT_RIGHT) {
    transform.Translate(center.x() - kBackgroundSizeDip / 2 - kIconOffsetDip,
                        center.y() - kBackgroundSizeDip / 2 + kIconOffsetDip);
  } else {
    DCHECK_EQ(alignment, SHELF_ALIGNMENT_LEFT);
    transform.Translate(center.x() - kBackgroundSizeDip / 2 + kIconOffsetDip,
                        center.y() - kBackgroundSizeDip / 2 + kIconOffsetDip);
  }
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(background_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullExpandDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    background_layer_->SetTransform(transform);
  }

  {
    ui::ScopedLayerAnimationSettings settings(background_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBackgroundStartDelayMs));
    background_layer_->SetVisible(true);

    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kBackgroundOpacityDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::PreemptionStrategy::ENQUEUE_NEW_ANIMATION);
    background_layer_->SetOpacity(1);
  }
}

void VoiceInteractionOverlay::BurstAnimation() {
  is_bursting_ = true;

  gfx::Point center = host_view_->GetCenterPoint();

  // Setup ripple animations.
  {
    SkMScalar scale_factor =
        kRippleCircleBurstRadiusDip / kRippleCircleInitRadiusDip;
    std::unique_ptr<gfx::Transform> transform(new gfx::Transform());
    transform->Translate(center.x() - kRippleCircleBurstRadiusDip,
                         center.y() - kRippleCircleBurstRadiusDip);
    transform->Scale(scale_factor, scale_factor);

    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullBurstDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    ripple_layer_->SetTransform(*transform.get());
    ripple_layer_->SetOpacity(0);
  }

  // Setup icon animation.
  {
    ui::ScopedLayerAnimationSettings settings(icon_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kIconBurstDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    icon_layer_->SetOpacity(0);
  }

  // Setup background animation.
  {
    ui::ScopedLayerAnimationSettings settings(background_layer_->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kIconBurstDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    background_layer_->SetOpacity(0);
  }
}

void VoiceInteractionOverlay::EndAnimation() {
  if (is_bursting_) {
    // Too late, user action already fired, we have to finish what's started.
    return;
  }

  // Play reverse animation
  // Setup ripple animations.
  SkMScalar scale_factor =
      kRippleCircleStartRadiusDip / kRippleCircleInitRadiusDip;
  gfx::Transform transform;

  const gfx::Point center = host_view_->GetCenterPoint();
  transform.Translate(center.x() - kRippleCircleStartRadiusDip,
                      center.y() - kRippleCircleStartRadiusDip);
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(ripple_layer_->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                       IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullRetractDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    ripple_layer_->SetTransform(transform);

    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kRippleOpacityRetractDurationMs));
    ripple_layer_->SetOpacity(0);
  }

  // Setup icon animation.
  transform.MakeIdentity();

  transform.Translate(center.x() - kIconStartSizeDip / 2.f,
                      center.y() - kIconStartSizeDip / 2.f);

  scale_factor = kIconStartSizeDip / kIconInitSizeDip;
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(icon_layer_->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                       IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullRetractDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    icon_layer_->SetTransform(transform);
    icon_layer_->SetOpacity(0);
  }

  // Setup background animation.
  transform.MakeIdentity();

  transform.Translate(center.x() - kBackgroundStartSizeDip / 2.f,
                      center.y() - kBackgroundStartSizeDip / 2.f);

  scale_factor = kBackgroundStartSizeDip / kBackgroundInitSizeDip;
  transform.Scale(scale_factor, scale_factor);

  {
    ui::ScopedLayerAnimationSettings settings(background_layer_->GetAnimator());
    settings.SetPreemptionStrategy(ui::LayerAnimator::PreemptionStrategy::
                                       IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kFullRetractDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    background_layer_->SetTransform(transform);
    background_layer_->SetOpacity(0);
  }
}

}  // namespace ash
