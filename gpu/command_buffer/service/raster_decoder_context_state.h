// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/progress_reporter.h"

namespace gl {
class GLContext;
class GLShareGroup;
class GLSurface;
}  // namespace gl

namespace viz {
class VulkanContextProvider;
}  // namespace viz

namespace gpu {
class GpuDriverBugWorkarounds;
class GpuProcessActivityFlags;
class ServiceTransferCache;

namespace raster {

struct GPU_GLES2_EXPORT RasterDecoderContextState
    : public base::RefCounted<RasterDecoderContextState>,
      public base::trace_event::MemoryDumpProvider {
 public:
  // TODO: Refactor code to have seperate constructor for GL and Vulkan and not
  // initialize/use GL related info for vulkan and vice-versa.
  RasterDecoderContextState(
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<gl::GLSurface> surface,
      scoped_refptr<gl::GLContext> context,
      bool use_virtualized_gl_contexts,
      viz::VulkanContextProvider* vulkan_context_provider = nullptr);

  void InitializeGrContext(const GpuDriverBugWorkarounds& workarounds,
                           GrContextOptions::PersistentCache* cache,
                           GpuProcessActivityFlags* activity_flags = nullptr,
                           gl::ProgressReporter* progress_reporter = nullptr);
  void PurgeMemory(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void PessimisticallyResetGrContext() const;

  scoped_refptr<gl::GLShareGroup> share_group;
  scoped_refptr<gl::GLSurface> surface;
  scoped_refptr<gl::GLContext> context;
  sk_sp<GrContext> owned_gr_context;
  std::unique_ptr<ServiceTransferCache> transfer_cache;
  const bool use_virtualized_gl_contexts = false;
  viz::VulkanContextProvider* vk_context_provider = nullptr;
  GrContext* gr_context = nullptr;
  const bool use_vulkan_gr_context = false;
  bool context_lost = false;
  size_t glyph_cache_max_texture_bytes = 0u;

  // |need_context_state_reset| is set whenever Skia may have altered the
  // driver's GL state. It signals the need to restore driver GL state to
  // |state_| before executing commands that do not
  // PermitsInconsistentContextState.
  bool need_context_state_reset = false;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class base::RefCounted<RasterDecoderContextState>;
  ~RasterDecoderContextState() override;
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_
