// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
#define CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_

#include "base/memory/ref_counted.h"
#include "cc/test/layer_tree_pixel_test.h"

namespace cc {

class LayerTreeHostImpl;
class RasterBufferProvider;
class ResourcePool;

// Enumerate the various combinations of renderer, resource pool, staging
// texture type, and drawing texture types.  Not all of the combinations
// are possible (or worth testing independently), so this is the minimal
// list to hit all codepaths.
enum PixelResourceTestCase {
  SOFTWARE,
  GL_GPU_RASTER_2D_DRAW,
  GL_ONE_COPY_2D_STAGING_2D_DRAW,
  GL_ONE_COPY_RECT_STAGING_2D_DRAW,
  GL_ONE_COPY_EXTERNAL_STAGING_2D_DRAW,
  GL_ZERO_COPY_2D_DRAW,
  GL_ZERO_COPY_RECT_DRAW,
  GL_ZERO_COPY_EXTERNAL_DRAW,
};

class LayerTreeHostPixelResourceTest : public LayerTreePixelTest {
 public:
  explicit LayerTreeHostPixelResourceTest(PixelResourceTestCase test_case);
  LayerTreeHostPixelResourceTest();

  void CreateResourceAndRasterBufferProvider(
      LayerTreeHostImpl* host_impl,
      std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
      std::unique_ptr<ResourcePool>* resource_pool) override;

  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            base::FilePath file_name);

  enum RasterBufferProviderType {
    RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY,
    RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY,
    RASTER_BUFFER_PROVIDER_TYPE_GPU,
    RASTER_BUFFER_PROVIDER_TYPE_BITMAP
  };

 protected:
  unsigned draw_texture_target_;
  RasterBufferProviderType raster_buffer_provider_type_;
  ResourceProvider::TextureHint texture_hint_;
  bool initialized_;

  void InitializeFromTestCase(PixelResourceTestCase test_case);

 private:
  PixelResourceTestCase test_case_;
};

#define INSTANTIATE_PIXEL_RESOURCE_TEST_CASE_P(framework_name)             \
  INSTANTIATE_TEST_CASE_P(                                                 \
      PixelResourceTest, framework_name,                                   \
      ::testing::Values(                                                   \
          SOFTWARE, GL_GPU_RASTER_2D_DRAW, GL_ONE_COPY_2D_STAGING_2D_DRAW, \
          GL_ONE_COPY_RECT_STAGING_2D_DRAW,                                \
          GL_ONE_COPY_EXTERNAL_STAGING_2D_DRAW, GL_ZERO_COPY_2D_DRAW,      \
          GL_ZERO_COPY_RECT_DRAW, GL_ZERO_COPY_EXTERNAL_DRAW))

class ParameterizedPixelResourceTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<PixelResourceTestCase> {
 public:
  ParameterizedPixelResourceTest();
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
