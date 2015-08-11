// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_layer_impl.h"

#include "cc/layers/layer.h"
#include "components/view_manager/public/cpp/view.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/mojo/geometry/geometry.mojom.h"

using blink::WebFloatPoint;
using blink::WebSize;

namespace html_viewer {

WebLayerImpl::WebLayerImpl(mojo::View* view, float device_pixel_ratio)
    : view_(view), device_pixel_ratio_(device_pixel_ratio) {}

WebLayerImpl::~WebLayerImpl() {
}

void WebLayerImpl::setBounds(const WebSize& size) {
  mojo::Rect rect = view_->bounds();
  const gfx::Size size_in_pixels(ConvertSizeToPixel(
      device_pixel_ratio_, gfx::Size(size.width, size.height)));
  rect.width = size_in_pixels.width();
  rect.height = size_in_pixels.height();
  view_->SetBounds(rect);
  cc_blink::WebLayerImpl::setBounds(size);
}

void WebLayerImpl::setPosition(const WebFloatPoint& position) {
  int x = 0, y = 0;
  // TODO(fsamuel): This is a temporary hack until we have a UI process in
  // Mandoline. The View will always lag behind the cc::Layer.
  cc::Layer* current_layer = layer();
  while (current_layer) {
    x += current_layer->position().x();
    x -= current_layer->scroll_offset().x();
    y += current_layer->position().y();
    y -= current_layer->scroll_offset().y();
    current_layer = current_layer->parent();
  }
  const gfx::Point point_in_pixels(
      ConvertPointToPixel(device_pixel_ratio_, gfx::Point(x, y)));
  mojo::Rect rect = view_->bounds();
  rect.x = point_in_pixels.x();
  rect.y = point_in_pixels.y();
  view_->SetBounds(rect);
  cc_blink::WebLayerImpl::setPosition(position);
}

}  // namespace html_viewer
