// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/minimized_home.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/canvas.h"

namespace {

const int kDragHandleWidth = 112;
const int kDragHandleHeight = 2;
const char kMinimizedHomeLayerName[] = "MinimizedHome";

class MinimizedHomePainter : public ui::LayerDelegate,
                             public ui::LayerOwner {
 public:
  explicit MinimizedHomePainter(ui::Layer* layer) {
    layer->set_name(kMinimizedHomeLayerName);
    layer->set_delegate(this);
    SetLayer(layer);
  }
  virtual ~MinimizedHomePainter() {}

 private:
  // ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    gfx::Rect bounds(layer()->GetTargetBounds().size());
    canvas->FillRect(bounds, SK_ColorBLACK);
    canvas->FillRect(gfx::Rect((bounds.width() - kDragHandleWidth) / 2,
                               bounds.bottom() - kDragHandleHeight,
                               kDragHandleWidth,
                               kDragHandleHeight),
                     SK_ColorWHITE);
  }

  virtual void OnDelegatedFrameDamage(
      const gfx::Rect& damage_rect_in_dip) OVERRIDE {
  }

  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  DISALLOW_COPY_AND_ASSIGN(MinimizedHomePainter);
};

}  // namespace

namespace athena {

scoped_ptr<ui::LayerOwner> CreateMinimizedHome() {
  return scoped_ptr<ui::LayerOwner>(
      new MinimizedHomePainter(new ui::Layer(ui::LAYER_TEXTURED)));
}

}  // namespace athena
