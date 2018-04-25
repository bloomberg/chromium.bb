// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {

namespace {

constexpr gfx::BufferFormat kBufferFormat = gfx::BufferFormat::RGBA_8888;
constexpr gfx::BufferUsage kBufferUsage = gfx::BufferUsage::SCANOUT;
constexpr viz::ResourceFormat kResourceFormat = viz::RGBA_8888;
constexpr gfx::Size kBufferSize(100, 100);

class RasterInProcessCommandBufferTest : public ::testing::TestWithParam<bool> {
 public:
  std::unique_ptr<RasterInProcessContext> CreateRasterInProcessContext() {
    ContextCreationAttribs attributes;
    attributes.enable_raster_interface = true;
    attributes.bind_generates_resource = false;

    // TODO(backer): Remove this once RasterDecoder is the default
    // implementation of RasterInterface.
    attributes.enable_gles2_interface = !GetParam();
    attributes.enable_raster_decoder = GetParam();

    auto context = std::make_unique<RasterInProcessContext>();
    auto result = context->Initialize(
        /*service=*/nullptr, attributes, SharedMemoryLimits(),
        gpu_memory_buffer_manager_.get(),
        /*image_factory=*/nullptr,
        /*gpu_channel_manager_delegate=*/nullptr,
        base::ThreadTaskRunnerHandle::Get());
    DCHECK_EQ(result, ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    gpu_memory_buffer_manager_ =
        std::make_unique<viz::TestGpuMemoryBufferManager>();
    context_ = CreateRasterInProcessContext();
    ri_ = context_->GetImplementation();
  }

  void TearDown() override {
    context_.reset();
    gpu_memory_buffer_manager_.reset();
  }

 protected:
  raster::RasterInterface* ri_;  // not owned
  std::unique_ptr<GpuMemoryBufferManager> gpu_memory_buffer_manager_;

 private:
  std::unique_ptr<RasterInProcessContext> context_;
};

}  // namespace

TEST_P(RasterInProcessCommandBufferTest, CreateImage) {
  // Calling CreateImageCHROMIUM() should allocate an image id.
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer1 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id1 = ri_->CreateImageCHROMIUM(
      gpu_memory_buffer1->AsClientBuffer(), kBufferSize.width(),
      kBufferSize.height(), GL_RGBA);

  EXPECT_GT(image_id1, 0u);

  // Create a second GLInProcessContext that is backed by a different
  // InProcessCommandBuffer. Calling CreateImageCHROMIUM() should return a
  // different id than the first call.
  std::unique_ptr<RasterInProcessContext> context2 =
      CreateRasterInProcessContext();
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer2 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id2 = context2->GetImplementation()->CreateImageCHROMIUM(
      buffer2->AsClientBuffer(), kBufferSize.width(), kBufferSize.height(),
      GL_RGBA);

  EXPECT_GT(image_id2, 0u);
  EXPECT_NE(image_id1, image_id2);
}

TEST_P(RasterInProcessCommandBufferTest, SetColorSpaceMetadata) {
  GLuint texture_id =
      ri_->CreateTexture(/*use_buffer=*/true, kBufferUsage, kResourceFormat);

  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer1 =
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          kBufferSize, kBufferFormat, kBufferUsage, kNullSurfaceHandle);
  GLuint image_id = ri_->CreateImageCHROMIUM(
      gpu_memory_buffer1->AsClientBuffer(), kBufferSize.width(),
      kBufferSize.height(), GL_RGBA);

  ri_->BindTexImage2DCHROMIUM(texture_id, image_id);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  gfx::ColorSpace color_space;
  ri_->SetColorSpaceMetadata(texture_id,
                             reinterpret_cast<GLColorSpace>(&color_space));
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());
}

INSTANTIATE_TEST_CASE_P(P, RasterInProcessCommandBufferTest, ::testing::Bool());

}  // namespace gpu
