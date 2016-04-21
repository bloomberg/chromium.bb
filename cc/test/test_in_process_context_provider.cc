// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_in_process_context_provider.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {

// static
std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext(
    TestGpuMemoryBufferManager* gpu_memory_buffer_manager,
    TestImageFactory* image_factory,
    gpu::GLInProcessContext* shared_context) {
  const bool is_offscreen = true;
  gpu::gles2::ContextCreationAttribHelper attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 24;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  gfx::GpuPreference gpu_preference = gfx::PreferDiscreteGpu;

  std::unique_ptr<gpu::GLInProcessContext> context =
      base::WrapUnique(gpu::GLInProcessContext::Create(
          nullptr, nullptr, is_offscreen, gfx::kNullAcceleratedWidget,
          gfx::Size(1, 1), shared_context, attribs, gpu_preference,
          gpu::SharedMemoryLimits(), gpu_memory_buffer_manager, image_factory));

  DCHECK(context);
  return context;
}

std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext() {
  return CreateTestInProcessContext(nullptr, nullptr, nullptr);
}

TestInProcessContextProvider::TestInProcessContextProvider(
    TestInProcessContextProvider* shared_context)
    : context_(CreateTestInProcessContext(
          &gpu_memory_buffer_manager_,
          &image_factory_,
          (shared_context ? shared_context->context_.get() : nullptr))) {}

TestInProcessContextProvider::~TestInProcessContextProvider() {
}

bool TestInProcessContextProvider::BindToCurrentThread() {
  return true;
}

gpu::gles2::GLES2Interface* TestInProcessContextProvider::ContextGL() {
  return context_->GetImplementation();
}

gpu::ContextSupport* TestInProcessContextProvider::ContextSupport() {
  return context_->GetImplementation();
}

class GrContext* TestInProcessContextProvider::GrContext() {
  if (gr_context_)
    return gr_context_->get();

  gr_context_.reset(new skia_bindings::GrContextForGLES2Interface(ContextGL()));
  return gr_context_->get();
}

void TestInProcessContextProvider::InvalidateGrContext(uint32_t state) {
  if (gr_context_)
    gr_context_->ResetContext(state);
}

void TestInProcessContextProvider::SetupLock() {
}

base::Lock* TestInProcessContextProvider::GetLock() {
  return &context_lock_;
}

gpu::Capabilities TestInProcessContextProvider::ContextCapabilities() {
  gpu::Capabilities capabilities;
  capabilities.image = true;
  capabilities.texture_rectangle = true;
  capabilities.sync_query = true;
  switch (PlatformColor::Format()) {
    case PlatformColor::SOURCE_FORMAT_RGBA8:
      capabilities.texture_format_bgra8888 = false;
      break;
    case PlatformColor::SOURCE_FORMAT_BGRA8:
      capabilities.texture_format_bgra8888 = true;
      break;
  }
  return capabilities;
}

void TestInProcessContextProvider::DeleteCachedResources() {
  if (gr_context_)
    gr_context_->FreeGpuResources();
}

void TestInProcessContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {}

}  // namespace cc
