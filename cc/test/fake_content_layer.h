// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_H_
#define CC_TEST_FAKE_CONTENT_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/content_layer.h"

namespace cc {

class FakeContentLayer : public ContentLayer {
 public:
  static scoped_refptr<FakeContentLayer> Create(ContentLayerClient* client) {
    return make_scoped_refptr(new FakeContentLayer(client));
  }

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  size_t update_count() const { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  size_t push_properties_count() const { return push_properties_count_; }
  void reset_push_properties_count() { push_properties_count_ = 0; }

  virtual bool Update(ResourceUpdateQueue* queue,
                      const OcclusionTracker<Layer>* occlusion) OVERRIDE;

  gfx::Rect LastPaintRect() const;

  void set_always_update_resources(bool always_update_resources) {
    always_update_resources_ = always_update_resources;
  }

  virtual void PushPropertiesTo(LayerImpl* layer) OVERRIDE;

  virtual void OnOutputSurfaceCreated() OVERRIDE;
  size_t output_surface_created_count() const {
    return output_surface_created_count_;
  }

  bool HaveBackingAt(int i, int j);

 private:
  explicit FakeContentLayer(ContentLayerClient* client);
  virtual ~FakeContentLayer();

  size_t update_count_;
  size_t push_properties_count_;
  size_t output_surface_created_count_;
  bool always_update_resources_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_H_
