// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_mask_layer_impl.h"

#include "base/memory/ptr_util.h"

namespace cc {

FakeMaskLayerImpl::FakeMaskLayerImpl(LayerTreeImpl* tree_impl,
                                     int id,
                                     scoped_refptr<RasterSource> raster_source,
                                     Layer::LayerMaskType mask_type)
    : PictureLayerImpl(tree_impl, id, mask_type) {
  SetBounds(raster_source->GetSize());
  Region region;
  UpdateRasterSource(raster_source, &region, nullptr);
}

std::unique_ptr<FakeMaskLayerImpl> FakeMaskLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<RasterSource> raster_source,
    Layer::LayerMaskType mask_type) {
  return base::WrapUnique(
      new FakeMaskLayerImpl(tree_impl, id, raster_source, mask_type));
}

void FakeMaskLayerImpl::GetContentsResourceId(ResourceId* resource_id,
                                              gfx::Size* resource_size) const {
  *resource_id = 0;
}

}  // namespace cc
