// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_resource_test.h"

#include "base/memory/ptr_util.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {

namespace {

bool IsTestCaseSupported(PixelResourceTestCase test_case) {
  switch (test_case) {
    case SOFTWARE:
    case GL_GPU_RASTER_2D_DRAW:
    case GL_ZERO_COPY_2D_DRAW:
    case GL_ZERO_COPY_RECT_DRAW:
    case GL_ONE_COPY_2D_STAGING_2D_DRAW:
    case GL_ONE_COPY_RECT_STAGING_2D_DRAW:
      return true;
    case GL_ZERO_COPY_EXTERNAL_DRAW:
    case GL_ONE_COPY_EXTERNAL_STAGING_2D_DRAW:
      // These should all be enabled in practice.
      // TODO(enne): look into getting texture external oes enabled.
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest(
    PixelResourceTestCase test_case)
    : draw_texture_target_(GL_INVALID_VALUE),
      raster_buffer_provider_type_(RASTER_BUFFER_PROVIDER_TYPE_BITMAP),
      texture_hint_(ResourceProvider::TEXTURE_HINT_IMMUTABLE),
      initialized_(false),
      test_case_(test_case) {
  InitializeFromTestCase(test_case);
}

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest()
    : draw_texture_target_(GL_INVALID_VALUE),
      raster_buffer_provider_type_(RASTER_BUFFER_PROVIDER_TYPE_BITMAP),
      initialized_(false),
      test_case_(SOFTWARE) {}

void LayerTreeHostPixelResourceTest::InitializeFromTestCase(
    PixelResourceTestCase test_case) {
  DCHECK(!initialized_);
  initialized_ = true;
  switch (test_case) {
    case SOFTWARE:
      test_type_ = PIXEL_TEST_SOFTWARE;
      draw_texture_target_ = GL_INVALID_VALUE;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_BITMAP;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_GPU_RASTER_2D_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_2D;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_GPU;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE_FRAMEBUFFER;
      return;
    case GL_ONE_COPY_2D_STAGING_2D_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_2D;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_ONE_COPY_RECT_STAGING_2D_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_2D;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_ONE_COPY_EXTERNAL_STAGING_2D_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_2D;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_ZERO_COPY_2D_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_2D;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_ZERO_COPY_RECT_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_RECTANGLE_ARB;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
    case GL_ZERO_COPY_EXTERNAL_DRAW:
      test_type_ = PIXEL_TEST_GL;
      draw_texture_target_ = GL_TEXTURE_EXTERNAL_OES;
      raster_buffer_provider_type_ = RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY;
      texture_hint_ = ResourceProvider::TEXTURE_HINT_IMMUTABLE;
      return;
  }
  NOTREACHED();
}

void LayerTreeHostPixelResourceTest::CreateResourceAndRasterBufferProvider(
    LayerTreeHostImpl* host_impl,
    std::unique_ptr<RasterBufferProvider>* raster_buffer_provider,
    std::unique_ptr<ResourcePool>* resource_pool) {
  base::SingleThreadTaskRunner* task_runner =
      task_runner_provider()->HasImplThread()
          ? task_runner_provider()->ImplThreadTaskRunner()
          : task_runner_provider()->MainThreadTaskRunner();
  DCHECK(task_runner);
  DCHECK(initialized_);

  ContextProvider* compositor_context_provider =
      host_impl->compositor_frame_sink()->context_provider();
  ContextProvider* worker_context_provider =
      host_impl->compositor_frame_sink()->worker_context_provider();
  ResourceProvider* resource_provider = host_impl->resource_provider();
  int max_bytes_per_copy_operation = 1024 * 1024;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;

  // Create resource pool.
  *resource_pool =
      ResourcePool::Create(resource_provider, task_runner, texture_hint_,
                           ResourcePool::kDefaultExpirationDelay);

  switch (raster_buffer_provider_type_) {
    case RASTER_BUFFER_PROVIDER_TYPE_BITMAP:
      EXPECT_FALSE(compositor_context_provider);
      EXPECT_EQ(PIXEL_TEST_SOFTWARE, test_type_);

      *raster_buffer_provider =
          BitmapRasterBufferProvider::Create(resource_provider);
      break;
    case RASTER_BUFFER_PROVIDER_TYPE_GPU:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      *raster_buffer_provider = base::MakeUnique<GpuRasterBufferProvider>(
          compositor_context_provider, worker_context_provider,
          resource_provider, false, 0, false);
      break;
    case RASTER_BUFFER_PROVIDER_TYPE_ZERO_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      *raster_buffer_provider = ZeroCopyRasterBufferProvider::Create(
          resource_provider, PlatformColor::BestTextureFormat());
      break;
    case RASTER_BUFFER_PROVIDER_TYPE_ONE_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_EQ(PIXEL_TEST_GL, test_type_);

      *raster_buffer_provider = base::MakeUnique<OneCopyRasterBufferProvider>(
          task_runner, compositor_context_provider, worker_context_provider,
          resource_provider, max_bytes_per_copy_operation, false,
          max_staging_buffer_usage_in_bytes, PlatformColor::BestTextureFormat(),
          false);
      break;
  }
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  if (!IsTestCaseSupported(test_case_))
    return;
  RunPixelTest(test_type_, content_root, file_name);
}

ParameterizedPixelResourceTest::ParameterizedPixelResourceTest()
    : LayerTreeHostPixelResourceTest(GetParam()) {
}

}  // namespace cc
