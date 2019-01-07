// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_
#define GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/trace_event/memory_dump_provider.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/command_buffer/service/gl_context_virtual_delegate.h"
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
namespace gles2 {
class FeatureInfo;
struct ContextState;
}  // namespace gles2

namespace raster {

// TODO(penghuang): Make RasterDecoderContextState a class.
struct GPU_GLES2_EXPORT RasterDecoderContextState
    : public base::trace_event::MemoryDumpProvider,
      public gpu::GLContextVirtualDelegate,
      public base::RefCounted<RasterDecoderContextState> {
 public:
  // TODO: Refactor code to have seperate constructor for GL and Vulkan and not
  // initialize/use GL related info for vulkan and vice-versa.
  RasterDecoderContextState(
      scoped_refptr<gl::GLShareGroup> share_group,
      scoped_refptr<gl::GLSurface> surface,
      scoped_refptr<gl::GLContext> context,
      bool use_virtualized_gl_contexts,
      base::OnceClosure context_lost_callback,
      viz::VulkanContextProvider* vulkan_context_provider = nullptr);

  void InitializeGrContext(const GpuDriverBugWorkarounds& workarounds,
                           GrContextOptions::PersistentCache* cache,
                           GpuProcessActivityFlags* activity_flags = nullptr,
                           gl::ProgressReporter* progress_reporter = nullptr);

  bool InitializeGL(scoped_refptr<gles2::FeatureInfo> feature_info);
  bool IsGLInitialized() const { return !!feature_info_; }

  bool MakeCurrent(gl::GLSurface* surface);
  void MarkContextLost();
  bool IsCurrent(gl::GLSurface* surface);

  void PurgeMemory(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  void PessimisticallyResetGrContext() const;

  gl::GLShareGroup* share_group() { return share_group_.get(); }
  gl::GLContext* context() { return context_.get(); }
  gl::GLContext* real_context() { return real_context_.get(); }
  gl::GLSurface* surface() { return surface_.get(); }
  gles2::FeatureInfo* feature_info() { return feature_info_.get(); }
  gles2::ContextState* context_state() const { return context_state_.get(); }
  bool context_lost() const { return context_lost_; }

  sk_sp<GrContext> owned_gr_context;
  std::unique_ptr<ServiceTransferCache> transfer_cache;
  const bool use_virtualized_gl_contexts = false;
  base::OnceClosure context_lost_callback;
  viz::VulkanContextProvider* vk_context_provider = nullptr;
  GrContext* gr_context = nullptr;
  const bool use_vulkan_gr_context = false;
  size_t glyph_cache_max_texture_bytes = 0u;
  std::vector<uint8_t> scratch_deserialization_buffer_;

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

  // gpu::GLContextVirtualDelegate implementation.
  bool initialized() const override;
  const gles2::ContextState* GetContextState() override;
  void RestoreState(const gles2::ContextState* prev_state) override;
  void RestoreGlobalState() const override;
  void ClearAllAttributes() const override;
  void RestoreActiveTexture() const override;
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override;
  void RestoreActiveTextureUnitBinding(unsigned int target) const override;
  void RestoreBufferBinding(unsigned int target) override;
  void RestoreBufferBindings() const override;
  void RestoreFramebufferBindings() const override;
  void RestoreRenderbufferBindings() override;
  void RestoreProgramBindings() const override;
  void RestoreTextureUnitBindings(unsigned unit) const override;
  void RestoreVertexAttribArray(unsigned index) override;
  void RestoreAllExternalTextureBindingsIfNeeded() override;
  QueryManager* GetQueryManager() override;

  scoped_refptr<gl::GLShareGroup> share_group_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLContext> real_context_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gles2::FeatureInfo> feature_info_;

  // raster decoders and display compositor share this context_state_.
  std::unique_ptr<gles2::ContextState> context_state_;

  bool context_lost_ = false;

  base::WeakPtrFactory<RasterDecoderContextState> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RasterDecoderContextState);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_RASTER_DECODER_CONTEXT_STATE_H_
