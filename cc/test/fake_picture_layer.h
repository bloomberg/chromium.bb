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

  // Layer implementation.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  bool Update() override;
  bool IsSuitableForGpuRasterization() const override;

  int update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  void set_force_unsuitable_for_gpu_rasterization(bool flag) {
    force_unsuitable_for_gpu_rasterization_ = flag;
  }

 private:
  explicit FakePictureLayer(ContentLayerClient* client);
  FakePictureLayer(ContentLayerClient* client,
                   std::unique_ptr<RecordingSource> source);
  ~FakePictureLayer() override;

  int update_count_;
  bool always_update_resources_;

  bool force_unsuitable_for_gpu_rasterization_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PICTURE_LAYER_H_
