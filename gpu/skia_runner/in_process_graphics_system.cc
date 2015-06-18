// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_runner/in_process_graphics_system.h"

#include <iostream>

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/thread_task_runner_handle.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/skia_bindings/gl_bindings_skia_cmd_buffer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"

namespace {

// TODO(hendrikw): Replace TestDiscardableMemoryAllocator and move to base?
class NonDiscardableMemory : public base::DiscardableMemory {
 public:
  explicit NonDiscardableMemory(size_t size) : data_(new uint8_t[size]) {}
  bool Lock() override { return false; }
  void Unlock() override {}
  void* data() const override { return data_.get(); }

 private:
  scoped_ptr<uint8_t[]> data_;
};

class NonDiscardableMemoryAllocator : public base::DiscardableMemoryAllocator {
 public:
  // Overridden from DiscardableMemoryAllocator:
  scoped_ptr<base::DiscardableMemory> AllocateLockedDiscardableMemory(
      size_t size) override {
    return make_scoped_ptr(new NonDiscardableMemory(size));
  }
};

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() { gles2::Initialize(); }
  ~GLES2Initializer() { gles2::Terminate(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<NonDiscardableMemoryAllocator> g_discardable;

}  // namespace anonymous

namespace skia_runner {

void BindContext3DGLContextCallback(const GrGLInterface* gl_interface) {
  gles2::SetGLContext(reinterpret_cast<gpu::GLInProcessContext*>(
                          gl_interface->fCallbackData)->GetImplementation());
}

InProcessGraphicsSystem::InProcessGraphicsSystem() {
  base::DiscardableMemoryAllocator::SetInstance(&g_discardable.Get());
  g_gles2_initializer.Get();

  if (!gfx::GLSurface::InitializeOneOff()) {
    LOG(ERROR) << "Unable to initialize gfx::GLSurface.";
    return;
  }

  // Create the in process context.
  in_process_context_ = CreateInProcessContext();
  if (!in_process_context_) {
    LOG(ERROR) << "Unable to create in process context.";
    return;
  }

  // Create and set up skia's GL bindings.
  gles2::SetGLContext(in_process_context_->GetImplementation());
  GrGLInterface* bindings = skia_bindings::CreateCommandBufferSkiaGLBinding();
  if (!bindings) {
    LOG(ERROR) << "Unable to create skia command buffer bindings.";
    return;
  }

  bindings->fCallback = BindContext3DGLContextCallback;
  bindings->fCallbackData =
      reinterpret_cast<GrGLInterfaceCallbackData>(in_process_context_.get());

  // Create skia's graphics context.
  gr_context_ = skia::AdoptRef(GrContext::Create(
      kOpenGL_GrBackend, reinterpret_cast<GrBackendContext>(bindings)));

  if (!gr_context_) {
    LOG(ERROR) << "Unable to create skia graphics context.";
    return;
  }

  // Set skia's graphics context cache size.
  const int kMaxGaneshResourceCacheCount = 2048;
  const size_t kMaxGaneshResourceCacheBytes = 96 * 1024 * 1024;
  gr_context_->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                      kMaxGaneshResourceCacheBytes);
}

InProcessGraphicsSystem::~InProcessGraphicsSystem() {
}

bool InProcessGraphicsSystem::IsSuccessfullyInitialized() const {
  return gr_context_;
}

int InProcessGraphicsSystem::GetMaxTextureSize() const {
  int max_texture_size = 0;
  in_process_context_->GetImplementation()->GetIntegerv(GL_MAX_TEXTURE_SIZE,
                                                        &max_texture_size);
  return max_texture_size;
}

scoped_ptr<gpu::GLInProcessContext>
InProcessGraphicsSystem::CreateInProcessContext() const {
  const bool is_offscreen = true;
  const bool share_resources = true;
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

  scoped_ptr<gpu::GLInProcessContext> context =
      make_scoped_ptr(gpu::GLInProcessContext::Create(
          nullptr, nullptr, is_offscreen, gfx::kNullAcceleratedWidget,
          gfx::Size(1, 1), nullptr, share_resources, attribs, gpu_preference,
          gpu::GLInProcessContextSharedMemoryLimits(), nullptr, nullptr));

  DCHECK(context);
  return context.Pass();
}

}  // namespace skia_runner
