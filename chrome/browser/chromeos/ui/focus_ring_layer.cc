// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/focus_ring_layer.h"

#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "base/bind.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const int kShadowRadius = 10;
const int kShadowAlpha = 90;
const SkColor kShadowColor = SkColorSetRGB(77, 144, 254);

}  // namespace

FocusRingLayer::FocusRingLayer()
    : window_(NULL),
      root_window_(NULL) {
}

FocusRingLayer::~FocusRingLayer() {}

void FocusRingLayer::Update() {
  if (!window_)
    return;

  aura::Window* root_window = window_->GetRootWindow();
  if (!layer_ || root_window != root_window_) {
    root_window_ = root_window;
    ui::Layer* root_layer = root_window->layer();
    layer_.reset(new ui::Layer(ui::LAYER_TEXTURED));
    layer_->set_name("FocusRing");
    layer_->set_delegate(this);
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
  }

  // Keep moving it to the top in case new layers have been added
  // since we created this layer.
  layer_->parent()->StackAtTop(layer_.get());

  // Translate native window coordinates to root window coordinates.
  gfx::Point origin = focus_ring_.origin();
  aura::Window::ConvertPointToTarget(window_, root_window_, &origin);
  gfx::Rect layer_bounds = focus_ring_;
  layer_bounds.set_origin(origin);
  int inset = -(kShadowRadius + 2);
  layer_bounds.Inset(inset, inset, inset, inset);
  layer_->SetBounds(layer_bounds);
}

void FocusRingLayer::SetForView(views::View* view) {
  if (!view) {
    if (layer_ && !focus_ring_.IsEmpty())
      layer_->SchedulePaint(focus_ring_);
    focus_ring_ = gfx::Rect();
    return;
  }

  DCHECK(view->GetWidget());
  window_ = view->GetWidget()->GetNativeWindow();

  gfx::Rect view_bounds = view->GetContentsBounds();

  // Workarounds that attempts to pick a better bounds.
  if (view->GetClassName() == views::LabelButton::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2, 2, 2, 2);
  }

  // Workarounds for system tray items that have customized focus borders.  The
  // insets here must be consistent with the ones used by those classes.
  if (view->GetClassName() == ash::ActionableView::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(1, 1, 3, 3);
  } else if (view->GetClassName() == ash::TrayBackgroundView::kViewClassName) {
    view_bounds.Inset(1, 1, 3, 3);
  } else if (view->GetClassName() ==
             ash::TrayPopupHeaderButton::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2, 1, 2, 2);
  }

  focus_ring_ = view->ConvertRectToWidget(view_bounds);
  Update();
}

void FocusRingLayer::OnPaintLayer(gfx::Canvas* canvas) {
  if (focus_ring_.IsEmpty())
    return;

  // Convert the focus ring from native-window-relative coordinates to
  // layer-relative coordinates.
  gfx::Point origin = focus_ring_.origin();
  aura::Window::ConvertPointToTarget(window_, root_window_, &origin);
  origin -= layer_->bounds().OffsetFromOrigin();
  gfx::Rect bounds = focus_ring_;
  bounds.set_origin(origin);

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

void FocusRingLayer::OnDeviceScaleFactorChanged(float device_scale_factor) {
  Update();
}

base::Closure FocusRingLayer::PrepareForLayerBoundsChange() {
  return base::Bind(&base::DoNothing);
}

}  // namespace chromeos
