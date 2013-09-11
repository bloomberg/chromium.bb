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
  return SolidColorScrollbarLayerImpl::Create(
      tree_impl, id(), orientation(), thumb_thickness_,
      is_left_side_vertical_scrollbar_).PassAs<LayerImpl>();
}

scoped_refptr<SolidColorScrollbarLayer> SolidColorScrollbarLayer::Create(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar,
    int scroll_layer_id) {
  return make_scoped_refptr(new SolidColorScrollbarLayer(
      orientation,
      thumb_thickness,
      is_left_side_vertical_scrollbar,
      scroll_layer_id));
}

SolidColorScrollbarLayer::SolidColorScrollbarLayer(
    ScrollbarOrientation orientation,
    int thumb_thickness,
    bool is_left_side_vertical_scrollbar,
    int scroll_layer_id)
    : scroll_layer_id_(scroll_layer_id),
      orientation_(orientation),
      thumb_thickness_(thumb_thickness),
      is_left_side_vertical_scrollbar_(is_left_side_vertical_scrollbar) {}

SolidColorScrollbarLayer::~SolidColorScrollbarLayer() {}

ScrollbarLayerInterface* SolidColorScrollbarLayer::ToScrollbarLayer() {
  return this;
}

bool SolidColorScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return true;
}

int SolidColorScrollbarLayer::ScrollLayerId() const {
  return scroll_layer_id_;
}

void SolidColorScrollbarLayer::SetScrollLayerId(int id) {
  if (id == scroll_layer_id_)
    return;

  scroll_layer_id_ = id;
  SetNeedsFullTreeSync();
}

ScrollbarOrientation SolidColorScrollbarLayer::orientation() const {
  return orientation_;
}

}  // namespace cc
