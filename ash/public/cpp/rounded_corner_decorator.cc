// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_corner_decorator.h"

#include "ash/public/cpp/ash_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash {

RoundedCornerDecorator::RoundedCornerDecorator(aura::Window* shadow_window,
                                               aura::Window* layer_window,
                                               ui::Layer* layer,
                                               int radius)
    : layer_window_(layer_window), layer_(layer), radius_(radius) {
  layer_window_->AddObserver(this);
  layer_->AddObserver(this);
  if (ash::features::ShouldUseShaderRoundedCorner()) {
    layer_->SetRoundedCornerRadius({radius_, radius_, radius_, radius_});
    layer_->SetIsFastRoundedCorner(true);
  } else {
    Update(layer_->size());
  }

  // Update the shadow if necessary.
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(shadow_window);
  if (shadow)
    shadow->SetRoundedCornerRadius(radius_);
}

RoundedCornerDecorator::~RoundedCornerDecorator() {
  Shutdown();
}

bool RoundedCornerDecorator::IsValid() {
  return !!layer_;
}

void RoundedCornerDecorator::OnPaintLayer(const ui::PaintContext& context) {
  cc::PaintFlags flags;
  flags.setAlpha(255);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);

  SkScalar radii[8] = {radius_, radius_,   // top-left
                       radius_, radius_,   // top-right
                       radius_, radius_,   // bottom-right
                       radius_, radius_};  // bottom-left
  SkPath path;
  const gfx::Size size = mask_layer_->size();
  path.addRoundRect(gfx::RectToSkRect(gfx::Rect(size)), radii);

  ui::PaintRecorder recorder(context, size);
  recorder.canvas()->DrawPath(path, flags);
}

void RoundedCornerDecorator::LayerDestroyed(ui::Layer* layer) {
  Shutdown();
}

void RoundedCornerDecorator::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  Update(new_bounds.size());
}

void RoundedCornerDecorator::OnWindowDestroying(aura::Window* window) {
  Shutdown();
}

void RoundedCornerDecorator::Update(const gfx::Size& size) {
  if (ash::features::ShouldUseShaderRoundedCorner())
    return;

  DCHECK(layer_window_);
  DCHECK(layer_);
  if (!mask_layer_) {
    mask_layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    mask_layer_->set_delegate(this);
    mask_layer_->SetFillsBoundsOpaquely(false);
    layer_->SetMaskLayer(mask_layer_.get());
  }
  mask_layer_->SetBounds(gfx::Rect(size));
}

void RoundedCornerDecorator::Shutdown() {
  if (!IsValid())
    return;
  if (ash::features::ShouldUseShaderRoundedCorner()) {
    layer_->SetRoundedCornerRadius({0, 0, 0, 0});
  } else {
    if (layer_->layer_mask_layer() == mask_layer_.get())
      layer_->SetMaskLayer(nullptr);
    mask_layer_.reset();
  }

  layer_->RemoveObserver(this);
  layer_window_->RemoveObserver(this);
  layer_ = nullptr;
  layer_window_ = nullptr;
}

}  // namespace ash
