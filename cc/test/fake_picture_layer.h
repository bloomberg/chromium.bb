// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_H_
#define CC_TEST_FAKE_PICTURE_LAYER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layers/picture_layer.h"
#include "cc/resources/recording_source.h"

namespace cc {
class FakePictureLayer : public PictureLayer {
 public:
  static scoped_refptr<FakePictureLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new FakePictureLayer(client));
  }

  static scoped_refptr<FakePictureLayer> CreateWithRecordingSource(
      ContentLayerClient* client,
      scoped_ptr<RecordingSource> source) {
    return make_scoped_refptr(new FakePictureLayer(client, source.Pass()));
  }

  scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  size_t update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  void disable_lcd_text() { disable_lcd_text_ = true; }

  bool Update(ResourceUpdateQueue* queue,
              const OcclusionTracker<Layer>* occlusion) override;

  void PushPropertiesTo(LayerImpl* layer) override;

  void OnOutputSurfaceCreated() override;
  size_t output_surface_created_count() const {
    return output_surface_created_count_;
  }

 private:
  explicit FakePictureLayer(ContentLayerClient* client);
  FakePictureLayer(ContentLayerClient* client,
                   scoped_ptr<RecordingSource> source);
  ~FakePictureLayer() override;

  size_t update_count_;
  size_t push_properties_count_;
  size_t output_surface_created_count_;
  bool always_update_resources_;
  bool disable_lcd_text_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_H_
