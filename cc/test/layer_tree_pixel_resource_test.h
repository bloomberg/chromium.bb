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

enum PixelResourceTestCase {
  SOFTWARE,
  GPU,
  ONE_COPY,
  ZERO_COPY,
};

class LayerTreeHostPixelResourceTest : public LayerTreePixelTest {
 public:
  explicit LayerTreeHostPixelResourceTest(PixelResourceTestCase test_case,
                                          Layer::LayerMaskType mask_type);
  LayerTreeHostPixelResourceTest();

  void CreateResourceAndRasterBufferProvider(
      LayerTreeHostImpl* host_impl,
      std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
      std::unique_ptr<ResourcePool>* resource_pool) override;

  void RunPixelResourceTest(scoped_refptr<Layer> content_root,
                            base::FilePath file_name);

 protected:
  PixelResourceTestCase test_case_;
  Layer::LayerMaskType mask_type_;
  bool initialized_ = false;

  void InitializeFromTestCase(PixelResourceTestCase test_case);
};

#define INSTANTIATE_PIXEL_RESOURCE_TEST_CASE_P(framework_name)         \
  INSTANTIATE_TEST_CASE_P(                                             \
      PixelResourceTest, framework_name,                               \
      ::testing::Combine(                                              \
          ::testing::Values(SOFTWARE, GPU, ONE_COPY, ZERO_COPY),       \
          ::testing::Values(Layer::LayerMaskType::SINGLE_TEXTURE_MASK, \
                            Layer::LayerMaskType::MULTI_TEXTURE_MASK)))

class ParameterizedPixelResourceTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<PixelResourceTestCase, Layer::LayerMaskType>> {
 public:
  ParameterizedPixelResourceTest();
};

}  // namespace cc

#endif  // CC_TEST_LAYER_TREE_PIXEL_RESOURCE_TEST_H_
