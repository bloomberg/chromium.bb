// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/throbber_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_throbber.h"
#include "ui/gfx/skia_util.h"

namespace chrome {
namespace android {

// static
scoped_refptr<ThrobberLayer> ThrobberLayer::Create(SkColor color) {
  return make_scoped_refptr(new ThrobberLayer(color));
}

void ThrobberLayer::Show(const base::Time& now) {
  if (!layer_->hide_layer_and_subtree())
    return;
  start_time_ = now;
  layer_->SetHideLayerAndSubtree(false);
}

void ThrobberLayer::Hide() {
  layer_->SetHideLayerAndSubtree(true);
}

void ThrobberLayer::SetColor(SkColor color) {
  color_ = color;
}

void ThrobberLayer::UpdateThrobber(const base::Time& now) {
  if (layer_->hide_layer_and_subtree())
    return;

  SkBitmap skbitmap;
  skbitmap.allocN32Pixels(layer_->bounds().width(), layer_->bounds().height());
  SkCanvas skcanvas(skbitmap);
  gfx::Canvas canvas(&skcanvas, 1.0f);
  gfx::Rect rect(0, 0, layer_->bounds().width(), layer_->bounds().height());
  SkPaint paint;
  paint.setAntiAlias(false);
  paint.setXfermodeMode(SkXfermode::kClear_Mode);
  SkRect skrect = RectToSkRect(rect);
  skcanvas.drawRect(skrect, paint);
  skcanvas.clipRect(skrect);
  gfx::PaintThrobberSpinning(&canvas, rect, color_, now - start_time_);
  skbitmap.setImmutable();
  layer_->SetBitmap(skbitmap);
}

scoped_refptr<cc::Layer> ThrobberLayer::layer() {
  return layer_;
}

ThrobberLayer::ThrobberLayer(SkColor color)
    : color_(color),
      start_time_(base::Time::Now()),
      layer_(
          cc::UIResourceLayer::Create(content::Compositor::LayerSettings())) {
  layer_->SetIsDrawable(true);
}

ThrobberLayer::~ThrobberLayer() {
}

}  //  namespace android
}  //  namespace chrome
