// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_scrollbar_layer_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/blink/scrollbar_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/layer.h"
#include "cc/layers/painted_overlay_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/scrollbar_layer_interface.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/trees/element_id.h"

using cc::PaintedOverlayScrollbarLayer;
using cc::PaintedScrollbarLayer;
using cc::SolidColorScrollbarLayer;

namespace {

cc::ScrollbarOrientation ConvertOrientation(
    blink::WebScrollbar::Orientation orientation) {
  return orientation == blink::WebScrollbar::kHorizontal ? cc::HORIZONTAL
                                                         : cc::VERTICAL;
}

}  // namespace

namespace cc_blink {

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    std::unique_ptr<blink::WebScrollbar> scrollbar,
    blink::WebScrollbarThemePainter painter,
    std::unique_ptr<blink::WebScrollbarThemeGeometry> geometry,
    bool is_overlay)
    : layer_(is_overlay
                 ? new WebLayerImpl(PaintedOverlayScrollbarLayer::Create(
                       std::make_unique<ScrollbarImpl>(std::move(scrollbar),
                                                       painter,
                                                       std::move(geometry)),
                       cc::ElementId()))
                 : new WebLayerImpl(PaintedScrollbarLayer::Create(
                       std::make_unique<ScrollbarImpl>(std::move(scrollbar),
                                                       painter,
                                                       std::move(geometry)),
                       cc::ElementId()))) {}

WebScrollbarLayerImpl::WebScrollbarLayerImpl(
    blink::WebScrollbar::Orientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar)
    : layer_(new WebLayerImpl(
          SolidColorScrollbarLayer::Create(ConvertOrientation(orientation),
                                           thumb_thickness,
                                           track_start,
                                           is_left_side_vertical_scrollbar,
                                           cc::ElementId()))) {}

WebScrollbarLayerImpl::~WebScrollbarLayerImpl() = default;

blink::WebLayer* WebScrollbarLayerImpl::Layer() {
  return layer_.get();
}

void WebScrollbarLayerImpl::SetScrollLayer(blink::WebLayer* layer) {
  cc::Layer* scroll_layer =
      layer ? static_cast<WebLayerImpl*>(layer)->layer() : nullptr;
  layer_->layer()->ToScrollbarLayer()->SetScrollElementId(
      scroll_layer ? scroll_layer->element_id() : cc::ElementId());
}

void WebScrollbarLayerImpl::SetElementId(const cc::ElementId& element_id) {
  layer_->SetElementId(element_id);
}

}  // namespace cc_blink
