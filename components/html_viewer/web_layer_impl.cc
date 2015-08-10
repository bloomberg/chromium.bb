// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_layer_impl.h"

#include "cc/layers/layer.h"
#include "components/view_manager/public/cpp/view.h"
#include "ui/mojo/geometry/geometry.mojom.h"

using blink::WebFloatPoint;
using blink::WebSize;

namespace html_viewer {

WebLayerImpl::WebLayerImpl(mojo::View* view) : view_(view) {}

WebLayerImpl::~WebLayerImpl() {
}

void WebLayerImpl::setBounds(const WebSize& size) {
  mojo::Rect rect = view_->bounds();
  rect.width = size.width;
  rect.height = size.height;
  view_->SetBounds(rect);
  cc_blink::WebLayerImpl::setBounds(size);
}

void WebLayerImpl::setPosition(const WebFloatPoint& position) {
  mojo::Rect rect = view_->bounds();
  rect.x = 0;
  rect.y = 0;
  // TODO(fsamuel): This is a temporary hack until we have a UI process in
  // Mandoline. The View will always lag behind the cc::Layer.
  cc::Layer* current_layer = layer();
  while (current_layer) {
    rect.x += current_layer->position().x();
    rect.x -= current_layer->scroll_offset().x();
    rect.y += current_layer->position().y();
    rect.y -= current_layer->scroll_offset().y();
    current_layer = current_layer->parent();
  }
  view_->SetBounds(rect);
  cc_blink::WebLayerImpl::setPosition(position);
}

}  // namespace html_viewer
