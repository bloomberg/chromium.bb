// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_REMOTE_CLIENT_LAYER_FACTORY_H_
#define CC_TEST_REMOTE_CLIENT_LAYER_FACTORY_H_

#include "base/macros.h"
#include "cc/blimp/layer_factory.h"

namespace cc {

// An implementation of LayerFactory for tests that ensures that the layer id
// for layers created on the client matches the ids of the corresponding layers
// on the engine.
class RemoteClientLayerFactory : public LayerFactory {
 public:
  RemoteClientLayerFactory();
  ~RemoteClientLayerFactory() override;

  // LayerFactory implementation.
  scoped_refptr<Layer> CreateLayer(int engine_layer_id) override;
  scoped_refptr<PictureLayer> CreatePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) override;
  scoped_refptr<PictureLayer> CreateFakePictureLayer(
      int engine_layer_id,
      ContentLayerClient* content_layer_client) override;
  scoped_refptr<SolidColorScrollbarLayer> CreateSolidColorScrollbarLayer(
      int engine_layer_id,
      ScrollbarOrientation orientation,
      int thumb_thickness,
      int track_start,
      bool is_left_side_vertical_scrollbar,
      int scroll_layer_id) override;
};

}  // namespace cc

#endif  // CC_TEST_REMOTE_CLIENT_LAYER_FACTORY_H_
