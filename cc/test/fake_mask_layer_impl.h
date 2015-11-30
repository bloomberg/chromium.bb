// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_MASK_LAYER_IMPL_H_
#define CC_TEST_FAKE_MASK_LAYER_IMPL_H_

#include "cc/layers/layer_impl.h"

namespace cc {

class FakeMaskLayerImpl : public LayerImpl {
 public:
  static scoped_ptr<FakeMaskLayerImpl> Create(LayerTreeImpl* tree_impl, int id);

  void GetContentsResourceId(ResourceId* resource_id,
                             gfx::Size* resource_size) const override;

 private:
  FakeMaskLayerImpl(LayerTreeImpl* tree_impl, int id);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_MASK_LAYER_IMPL_H_
