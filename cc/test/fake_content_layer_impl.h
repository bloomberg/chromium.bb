// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_CONTENT_LAYER_IMPL_H_
#define CC_TEST_FAKE_CONTENT_LAYER_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "cc/layers/tiled_layer_impl.h"

namespace cc {

class FakeContentLayerImpl : public TiledLayerImpl {
 public:
  static scoped_ptr<FakeContentLayerImpl> Create(
      LayerTreeImpl* tree_impl, int id) {
    return make_scoped_ptr(new FakeContentLayerImpl(tree_impl, id));
  }
  virtual ~FakeContentLayerImpl();

  virtual scoped_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl)
      OVERRIDE;

  bool HaveResourceForTileAt(int i, int j);

  size_t lost_output_surface_count() const {
    return lost_output_surface_count_;
  }
  void reset_lost_output_surface_count() { lost_output_surface_count_ = 0; }

  virtual void ReleaseResources() OVERRIDE;

 private:
  explicit FakeContentLayerImpl(LayerTreeImpl* tree_impl, int id);

  size_t lost_output_surface_count_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_CONTENT_LAYER_IMPL_H_
