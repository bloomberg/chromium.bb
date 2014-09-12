// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/focus_ring_layer.h"

#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"

namespace chromeos {

namespace {

const int kShadowRadius = 10;
const int kShadowAlpha = 90;
const SkColor kShadowColor = SkColorSetRGB(77, 144, 254);

}  // namespace

FocusRingLayerDelegate::~FocusRingLayerDelegate() {}

FocusRingLayer::FocusRingLayer(FocusRingLayerDelegate* delegate)
    : delegate_(delegate),
      root_window_(NULL) {
}

FocusRingLayer::~FocusRingLayer() {}

void FocusRingLayer::Set(aura::Window* root_window, const gfx::Rect& bounds) {
  focus_ring_ = bounds;
  CreateOrUpdateLayer(root_window, "FocusRing");

  // Update the layer bounds.
  gfx::Rect layer_bounds = bounds;
  int inset = -(kShadowRadius + 2);
  layer_bounds.Inset(inset, inset, inset, inset);
  layer_->SetBounds(layer_bounds);
}

void FocusRingLayer::CreateOrUpdateLayer(
    aura::Window* root_window, const char* layer_name) {
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
}

void FocusRingLayer::OnPaintLayer(gfx::Canvas* canvas) {
  if (!root_window_ || focus_ring_.IsEmpty())
    return;

  gfx::Rect bounds = focus_ring_ - layer_->bounds().OffsetFromOrigin();
  SkPaint paint;
  paint.setColor(kShadowColor);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(2);
  int r = kShadowRadius;
  for (int i = 0; i < r; i++) {
    // Fade out alpha quadratically.
    paint.setAlpha((kShadowAlpha * (r - i) * (r - i)) / (r * r));
    gfx::Rect outsetRect = bounds;
    outsetRect.Inset(-i, -i, -i, -i);
    canvas->DrawRect(outsetRect, paint);
  }
}

void FocusRingLayer::OnDelegatedFrameDamage(
    const gfx::Rect& damage_rect_in_dip) {
}

void FocusRingLayer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged();
}

base::Closure FocusRingLayer::PrepareForLayerBoundsChange() {
  return base::Bind(&base::DoNothing);
}

}  // namespace chromeos
