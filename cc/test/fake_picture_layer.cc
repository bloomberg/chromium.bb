// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_layer.h"

#include "cc/test/fake_picture_layer_impl.h"

namespace cc {

FakePictureLayer::FakePictureLayer(ContentLayerClient* client)
    : PictureLayer(client),
      update_count_(0),
      push_properties_count_(0),
      output_surface_created_count_(0),
      always_update_resources_(false),
      disable_lcd_text_(false) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::FakePictureLayer(ContentLayerClient* client,
                                   scoped_ptr<RecordingSource> source)
    : PictureLayer(client, source.Pass()),
      update_count_(0),
      push_properties_count_(0),
      output_surface_created_count_(0),
      always_update_resources_(false),
      disable_lcd_text_(false) {
  SetBounds(gfx::Size(1, 1));
  SetIsDrawable(true);
}

FakePictureLayer::~FakePictureLayer() {}

scoped_ptr<LayerImpl> FakePictureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return FakePictureLayerImpl::Create(tree_impl, layer_id_);
}

bool FakePictureLayer::Update(ResourceUpdateQueue* queue,
                              const OcclusionTracker<Layer>* occlusion) {
  if (disable_lcd_text_)
    draw_properties().can_use_lcd_text = false;
  bool updated = PictureLayer::Update(queue, occlusion);
  update_count_++;
  return updated || always_update_resources_;
}

void FakePictureLayer::PushPropertiesTo(LayerImpl* layer) {
  PictureLayer::PushPropertiesTo(layer);
  push_properties_count_++;
}

void FakePictureLayer::OnOutputSurfaceCreated() {
  PictureLayer::OnOutputSurfaceCreated();
  output_surface_created_count_++;
}

}  // namespace cc
