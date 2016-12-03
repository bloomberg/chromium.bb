// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_in_process_context_provider.h"

#include <stdint.h>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/context_cache_controller.h"
#include "cc/resources/platform_color.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/gl_in_process_context.h"
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
    gpu::GLInProcessContext* shared_context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  const bool is_offscreen = true;
  gpu::gles2::ContextCreationAttribHelper attribs;
  attribs.alpha_size = -1;
  attribs.depth_size = 24;
  attribs.stencil_size = 8;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;

  std::unique_ptr<gpu::GLInProcessContext> context =
      base::WrapUnique(gpu::GLInProcessContext::Create(
          nullptr, nullptr, is_offscreen, gpu::kNullSurfaceHandle,
          shared_context, attribs, gpu::SharedMemoryLimits(),
          gpu_memory_buffer_manager, image_factory, std::move(task_runner)));

  DCHECK(context);
  return context;
}

std::unique_ptr<gpu::GLInProcessContext> CreateTestInProcessContext() {
  return CreateTestInProcessContext(nullptr, nullptr, nullptr,
                                    base::ThreadTaskRunnerHandle::Get());
}

TestInProcessContextProvider::TestInProcessContextProvider(
    TestInProcessContextProvider* shared_context) {
  context_ = CreateTestInProcessContext(
      &gpu_memory_buffer_manager_, &image_factory_,
      (shared_context ? shared_context->context_.get() : nullptr),
      base::ThreadTaskRunnerHandle::Get());
  cache_controller_.reset(new ContextCacheController(
      context_->GetImplementation(), base::ThreadTaskRunnerHandle::Get()));
}

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
  cache_controller_->SetGrContext(gr_context_->get());
  return gr_context_->get();
}

ContextCacheController* TestInProcessContextProvider::CacheController() {
  return cache_controller_.get();
}

void TestInProcessContextProvider::InvalidateGrContext(uint32_t state) {
  if (gr_context_)
    gr_context_->ResetContext(state);
}

base::Lock* TestInProcessContextProvider::GetLock() {
  return &context_lock_;
}

gpu::Capabilities TestInProcessContextProvider::ContextCapabilities() {
  gpu::Capabilities capabilities;
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

void TestInProcessContextProvider::SetLostContextCallback(
    const LostContextCallback& lost_context_callback) {}

}  // namespace cc
