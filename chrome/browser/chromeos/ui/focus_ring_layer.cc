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
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

const int kShadowRadius = 23;
const int kCenterBlockSize = 2 * kShadowRadius;
const int kFocusRingImageSize = kShadowRadius * 2 + kCenterBlockSize;
const SkColor kShadowColor = SkColorSetRGB(77, 144, 254);

// FocusRingImageSource generates a base image that could be used by
// ImagePainter to paint a focus ring around a rect. The base image is a
// transparent square block of kCenterBlockSize pixels with blue halo around.
class FocusRingImageSource : public gfx::CanvasImageSource {
 public:
  FocusRingImageSource()
      : CanvasImageSource(gfx::Size(kFocusRingImageSize, kFocusRingImageSize),
                          false) {
    shadows_.push_back(
        gfx::ShadowValue(gfx::Point(), kShadowRadius, kShadowColor));
  }

  virtual void Draw(gfx::Canvas* canvas) OVERRIDE {
    SkPaint paint;
    paint.setColor(kShadowColor);

    skia::RefPtr<SkDrawLooper> looper = CreateShadowDrawLooper(shadows_);
    paint.setLooper(looper.get());

    const gfx::Rect rect(kShadowRadius, kShadowRadius,
                         kCenterBlockSize, kCenterBlockSize);
    canvas->DrawRect(rect, paint);
    canvas->FillRect(rect, SK_ColorTRANSPARENT, SkXfermode::kSrc_Mode);
  }

 private:
  gfx::ShadowValues shadows_;

  DISALLOW_COPY_AND_ASSIGN(FocusRingImageSource);
};

}  // namespace

FocusRingLayer::FocusRingLayer() {
  gfx::ImageSkia ring_image(
      new FocusRingImageSource,
      gfx::Size(kFocusRingImageSize, kFocusRingImageSize));
  ring_painter_.reset(views::Painter::CreateImagePainter(
      ring_image,
      gfx::Insets(kShadowRadius, kShadowRadius, kShadowRadius, kShadowRadius)));
}

FocusRingLayer::~FocusRingLayer() {}

void FocusRingLayer::SetForView(views::View* view) {
  if (!view ||
      !view->GetWidget() ||
      !view->GetWidget()->GetNativeWindow() ||
      !view->GetWidget()->GetNativeWindow()->layer()) {
    layer_.reset();
    return;
  }

  if (!layer_) {
    layer_.reset(new ui::Layer(ui::LAYER_TEXTURED));
    layer_->set_name("FocusRing");
    layer_->set_delegate(this);
    layer_->SetFillsBoundsOpaquely(false);
  }

  // Puts the focus ring layer as a sibling layer of the widget layer so that
  // it does not clip at the widget's boundary.
  ui::Layer* widget_layer = view->GetWidget()->GetNativeWindow()->layer();
  widget_layer->parent()->Add(layer_.get());

  // Workarounds that attempts to pick a better bounds.
  gfx::Rect view_bounds = view->GetContentsBounds();
  if (view->GetClassName() == views::LabelButton::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2, 2, 2, 2);
  }

  // Workarounds for system tray items that has a customized OnPaintFocusBorder.
  // The insets here must be consistent with the ones used in OnPaintFocusBorder
  // and DrawBorder.
  if (view->GetClassName() ==
             ash::internal::ActionableView::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(1, 1, 3, 3);
  } else if (view->GetClassName() ==
             ash::internal::TrayBackgroundView::kViewClassName) {
    view_bounds.Inset(1, 1, 3, 3);
  } else if (view->GetClassName() ==
             ash::internal::TrayPopupHeaderButton::kViewClassName) {
    view_bounds = view->GetLocalBounds();
    view_bounds.Inset(2, 1, 2, 2);
  }

  // Note the bounds calculation below assumes no transformation and ignores
  // animations.
  const gfx::Rect widget_bounds = widget_layer->GetTargetBounds();
  gfx::Rect bounds = view->ConvertRectToWidget(view_bounds);
  bounds.Offset(widget_bounds.OffsetFromOrigin());
  bounds.Inset(-kShadowRadius, -kShadowRadius, -kShadowRadius, -kShadowRadius);
  layer_->SetBounds(bounds);
}

void FocusRingLayer::OnPaintLayer(gfx::Canvas* canvas) {
  ring_painter_->Paint(canvas, layer_->bounds().size());
}

void FocusRingLayer::OnDeviceScaleFactorChanged(float device_scale_factor) {
}

base::Closure FocusRingLayer::PrepareForLayerBoundsChange() {
  return base::Bind(&base::DoNothing);
}

}  // namespace chromeos
