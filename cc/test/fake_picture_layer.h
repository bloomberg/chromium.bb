// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PICTURE_LAYER_H_
#define CC_TEST_FAKE_PICTURE_LAYER_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "cc/layers/picture_layer.h"
#include "cc/playback/recording_source.h"

namespace cc {
class FakePictureLayer : public PictureLayer {
 public:
  static scoped_refptr<FakePictureLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new FakePictureLayer(client));
  }

  static scoped_refptr<FakePictureLayer> CreateWithRecordingSource(
      ContentLayerClient* client,
      std::unique_ptr<RecordingSource> source) {
    return make_scoped_refptr(new FakePictureLayer(client, std::move(source)));
  }

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  bool Update() override;

  void PushPropertiesTo(LayerImpl* layer) override;

  void set_recorded_viewport(const gfx::Rect& recorded_viewport) {
    picture_layer_inputs_.recorded_viewport = recorded_viewport;
  }

  DisplayItemList* display_list() const {
    return picture_layer_inputs_.display_list.get();
  }

  void set_force_unsuitable_for_gpu_rasterization(bool flag) {
    force_unsuitable_for_gpu_rasterization_ = flag;
  }

  bool IsSuitableForGpuRasterization() const override;

 private:
  explicit FakePictureLayer(ContentLayerClient* client);
  FakePictureLayer(ContentLayerClient* client,
                   std::unique_ptr<RecordingSource> source);
  ~FakePictureLayer() override;

  int update_count_;
  size_t push_properties_count_;
  bool always_update_resources_;

  bool force_unsuitable_for_gpu_rasterization_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_H_
