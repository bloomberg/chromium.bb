// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/remote_client_layer_factory.h"

#include "cc/layers/layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_scrollbar_layer.h"
#include "cc/test/fake_picture_layer.h"

namespace cc {

RemoteClientLayerFactory::RemoteClientLayerFactory() = default;

RemoteClientLayerFactory::~RemoteClientLayerFactory() = default;

scoped_refptr<Layer> RemoteClientLayerFactory::CreateLayer(
    int engine_layer_id) {
  scoped_refptr<Layer> layer = Layer::Create();
  layer->SetLayerIdForTesting(engine_layer_id);
  return layer;
}

scoped_refptr<PictureLayer> RemoteClientLayerFactory::CreatePictureLayer(
    int engine_layer_id,
    ContentLayerClient* content_layer_client) {
  scoped_refptr<PictureLayer> layer =
      PictureLayer::Create(content_layer_client);
  layer->SetLayerIdForTesting(engine_layer_id);
  return layer;
}

scoped_refptr<PictureLayer> RemoteClientLayerFactory::CreateFakePictureLayer(
    int engine_layer_id,
    ContentLayerClient* content_layer_client) {
  scoped_refptr<PictureLayer> layer =
      FakePictureLayer::Create(content_layer_client);
  layer->SetLayerIdForTesting(engine_layer_id);
  return layer;
}

scoped_refptr<SolidColorScrollbarLayer>
RemoteClientLayerFactory::CreateSolidColorScrollbarLayer(
    int engine_layer_id,
    ScrollbarOrientation orientation,
    int thumb_thickness,
    int track_start,
    bool is_left_side_vertical_scrollbar,
    int scroll_layer_id) {
  scoped_refptr<SolidColorScrollbarLayer> layer =
      SolidColorScrollbarLayer::Create(
          orientation, thumb_thickness, track_start,
          is_left_side_vertical_scrollbar, scroll_layer_id);
  layer->SetLayerIdForTesting(engine_layer_id);
  return layer;
}

}  // namespace cc
