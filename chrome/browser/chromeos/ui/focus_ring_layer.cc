// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/focus_ring_layer.h"

#include "ui/aura/window.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"

namespace ui {
class Compositor;
}

namespace chromeos {

namespace {

const int kShadowRadius = 10;
const int kShadowAlpha = 90;
const SkColor kShadowColor = SkColorSetRGB(77, 144, 254);

}  // namespace

FocusRingLayerDelegate::~FocusRingLayerDelegate() {}

FocusRingLayer::FocusRingLayer(FocusRingLayerDelegate* delegate)
    : delegate_(delegate) {}

FocusRingLayer::~FocusRingLayer() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);
}

void FocusRingLayer::Set(aura::Window* root_window, const gfx::Rect& bounds) {
  focus_ring_ = bounds;
  gfx::Rect layer_bounds = bounds;
  int inset = -(kShadowRadius + 2);
  layer_bounds.Inset(inset, inset, inset, inset);
  CreateOrUpdateLayer(root_window, "FocusRing", layer_bounds);
}

bool FocusRingLayer::CanAnimate() const {
  return compositor_ && compositor_->HasAnimationObserver(this);
}

void FocusRingLayer::SetOpacity(float opacity) {
  layer()->SetOpacity(opacity);
}

void FocusRingLayer::SetColor(SkColor color) {
  custom_color_ = color;
}

void FocusRingLayer::ResetColor() {
  custom_color_.reset();
}

void FocusRingLayer::CreateOrUpdateLayer(aura::Window* root_window,
                                         const char* layer_name,
                                         const gfx::Rect& bounds) {
  if (!layer_ || root_window != root_window_) {
    root_window_ = root_window;
    ui::Layer* root_layer = root_window->layer();
    layer_.reset(new ui::Layer(ui::LAYER_TEXTURED));
    layer_->set_name(layer_name);
    layer_->set_delegate(this);
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
  }

  // Keep moving it to the top in case new layers have been added
  // since we created this layer.
  layer_->parent()->StackAtTop(layer_.get());

  layer_->SetBounds(bounds);
  gfx::Rect layer_bounds(0, 0, bounds.width(), bounds.height());
  layer_->SchedulePaint(layer_bounds);

  // Update the animation observer.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayMatching(bounds);
  ui::Compositor* compositor = root_window->layer()->GetCompositor();
  if (compositor != compositor_) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = compositor;
    if (compositor_ && !compositor_->HasAnimationObserver(this))
      compositor_->AddAnimationObserver(this);
  }
}

void FocusRingLayer::OnPaintLayer(const ui::PaintContext& context) {
  if (!root_window_ || focus_ring_.IsEmpty())
    return;

  ui::PaintRecorder recorder(context, layer_->size());

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(custom_color_ ? *custom_color_ : kShadowColor);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(2);

  gfx::Rect bounds = focus_ring_ - layer_->bounds().OffsetFromOrigin();
  int r = kShadowRadius;
  for (int i = 0; i < r; i++) {
    // Fade out alpha quadratically.
    flags.setAlpha((kShadowAlpha * (r - i) * (r - i)) / (r * r));
    gfx::Rect outsetRect = bounds;
    outsetRect.Inset(-i, -i, -i, -i);
    recorder.canvas()->DrawRect(outsetRect, flags);
  }
}

void FocusRingLayer::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void FocusRingLayer::OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                                float new_device_scale_factor) {
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged();
}

void FocusRingLayer::OnAnimationStep(base::TimeTicks timestamp) {
  delegate_->OnAnimationStep(timestamp);
}

void FocusRingLayer::OnCompositingShuttingDown(ui::Compositor* compositor) {
  if (compositor == compositor_) {
    compositor->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

}  // namespace chromeos
