// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_layer_impl.h"

#include "base/bind.h"
#include "cc/layers/layer.h"
#include "components/mus/public/cpp/view.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/mojo/geometry/geometry.mojom.h"

using blink::WebFloatPoint;
using blink::WebSize;

namespace html_viewer {

namespace {

// See surface_layer.h for a description of this callback.
void SatisfyCallback(cc::SurfaceSequence sequence) {
  // TODO(fsamuel): Implement this.
}

// See surface_layer.h for a description of this callback.
void RequireCallback(cc::SurfaceId surface_id,
                     cc::SurfaceSequence sequence) {
  // TODO(fsamuel): Implement this.
}

}

WebLayerImpl::WebLayerImpl(mus::View* view, float device_pixel_ratio)
    : cc_blink::WebLayerImpl(
          cc::SurfaceLayer::Create(cc_blink::WebLayerImpl::LayerSettings(),
                                   base::Bind(&SatisfyCallback),
                                   base::Bind(&RequireCallback))),
      view_(view),
      device_pixel_ratio_(device_pixel_ratio) {}

WebLayerImpl::~WebLayerImpl() {
}

void WebLayerImpl::setBounds(const WebSize& size) {
  gfx::Size size_in_pixels =
      gfx::ScaleToCeiledSize(gfx::Size(size), device_pixel_ratio_);
  static_cast<cc::SurfaceLayer*>(layer())->
      SetSurfaceId(cc::SurfaceId(view_->id()),
                   device_pixel_ratio_, size_in_pixels);
  cc_blink::WebLayerImpl::setBounds(size);
}

}  // namespace html_viewer
