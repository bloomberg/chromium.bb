// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blink/web_compositor_support_impl.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/blink/web_content_layer_impl.h"
#include "cc/blink/web_display_item_list_impl.h"
#include "cc/blink/web_external_texture_layer_impl.h"
#include "cc/blink/web_image_layer_impl.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/blink/web_scrollbar_layer_impl.h"
#include "cc/layers/layer.h"

using blink::WebContentLayer;
using blink::WebContentLayerClient;
using blink::WebDisplayItemList;
using blink::WebExternalTextureLayer;
using blink::WebImageLayer;
using blink::WebLayer;
using blink::WebScrollbar;
using blink::WebScrollbarLayer;
using blink::WebScrollbarThemeGeometry;
using blink::WebScrollbarThemePainter;

namespace cc_blink {

WebCompositorSupportImpl::WebCompositorSupportImpl() = default;

WebCompositorSupportImpl::~WebCompositorSupportImpl() = default;

std::unique_ptr<WebLayer> WebCompositorSupportImpl::CreateLayer() {
  return std::make_unique<WebLayerImpl>();
}

std::unique_ptr<WebLayer> WebCompositorSupportImpl::CreateLayerFromCCLayer(
    cc::Layer* layer) {
  return std::make_unique<WebLayerImpl>(layer);
}

std::unique_ptr<WebContentLayer> WebCompositorSupportImpl::CreateContentLayer(
    WebContentLayerClient* client) {
  return std::make_unique<WebContentLayerImpl>(client);
}

std::unique_ptr<WebExternalTextureLayer>
WebCompositorSupportImpl::CreateExternalTextureLayer(
    cc::TextureLayerClient* client) {
  return std::make_unique<WebExternalTextureLayerImpl>(client);
}

std::unique_ptr<blink::WebImageLayer>
WebCompositorSupportImpl::CreateImageLayer() {
  return std::make_unique<WebImageLayerImpl>();
}

std::unique_ptr<WebScrollbarLayer>
WebCompositorSupportImpl::CreateScrollbarLayer(
    std::unique_ptr<WebScrollbar> scrollbar,
    WebScrollbarThemePainter painter,
    std::unique_ptr<WebScrollbarThemeGeometry> geometry) {
  return std::make_unique<WebScrollbarLayerImpl>(std::move(scrollbar), painter,
                                                 std::move(geometry),
                                                 /* is overlay */ false);
}

std::unique_ptr<WebScrollbarLayer>
WebCompositorSupportImpl::CreateOverlayScrollbarLayer(
    std::unique_ptr<WebScrollbar> scrollbar,
    WebScrollbarThemePainter painter,
    std::unique_ptr<WebScrollbarThemeGeometry> geometry) {
  return std::make_unique<WebScrollbarLayerImpl>(std::move(scrollbar), painter,
                                                 std::move(geometry),
                                                 /* is overlay */ true);
}

std::unique_ptr<WebScrollbarLayer>
WebCompositorSupportImpl::CreateSolidColorScrollbarLayer(
    WebScrollbar::Orientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar) {
  return std::make_unique<WebScrollbarLayerImpl>(
      orientation, thumb_thickness, track_start,
      is_left_side_vertical_scrollbar);
}

}  // namespace cc_blink
