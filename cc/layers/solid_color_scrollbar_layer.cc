// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/solid_color_scrollbar_layer.h"

#include "base/memory/scoped_ptr.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"

namespace cc {

scoped_ptr<LayerImpl> SolidColorScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  const bool kIsOverlayScrollbar = true;
  return SolidColorScrollbarLayerImpl::Create(
             tree_impl,
             id(),
             orientation(),
             thumb_thickness_,
             is_left_side_vertical_scrollbar_,
             kIsOverlayScrollbar).PassAs<LayerImpl>();
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar,
    Layer* scroll_layer) {
  return make_scoped_refptr(new SolidColorScrollbarLayer(
      orientation,
      thumb_thickness,
      is_left_side_vertical_scrollbar,
      scroll_layer));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar,
    Layer* scroll_layer)
    : scroll_layer_(scroll_layer),
      clip_layer_(NULL),
      orientation_(orientation),
      thumb_thickness_(thumb_thickness),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar) {}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() {}

ScrollbarLayerInterface* SolidColorScrollbarLayer::ToScrollbarLayer() {
  return this;
}

void SolidColorScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  PushScrollClipPropertiesTo(layer);
}

void SolidColorScrollbarLayer::PushScrollClipPropertiesTo(LayerImpl* layer) {
  SolidColorScrollbarLayerImpl* scrollbar_layer =
      static_cast<SolidColorScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollLayerById(scroll_layer_ ? scroll_layer_->id()
                                                    : Layer::INVALID_ID);
  scrollbar_layer->SetClipLayerById(clip_layer_ ? clip_layer_->id()
                                                : Layer::INVALID_ID);
}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

int SolidColorScrollbarLayer::ScrollLayerId() const {
  return scroll_layer_->id();
}

void SolidColorScrollbarLayer::SetScrollLayer(scoped_refptr<Layer> layer) {
  if (layer == scroll_layer_)
    return;

  scroll_layer_ = layer;
  SetNeedsFullTreeSync();
}

void SolidColorScrollbarLayer::SetClipLayer(scoped_refptr<Layer> layer) {
  if (layer == clip_layer_)
    return;

  clip_layer_ = layer;
  SetNeedsFullTreeSync();
}

ScrollbarOrientation SolidColorScrollbarLayer::orientation() const {
  return orientation_;
}

}  // namespace cc
