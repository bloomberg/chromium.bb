// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/raster_decoder_context_state.h"

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/common/activity_flags.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/create_gr_gl_interface.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#endif

namespace {
static constexpr size_t kInitialScratchDeserializationBufferSize = 1024;
}

namespace gpu {
namespace raster {

RasterDecoderContextState::RasterDecoderContextState(
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<gl::GLSurface> surface,
    scoped_refptr<gl::GLContext> context,
    bool use_virtualized_gl_contexts,
    base::OnceClosure context_lost_callback,
    viz::VulkanContextProvider* vulkan_context_provider)
    : use_virtualized_gl_contexts(use_virtualized_gl_contexts),
      context_lost_callback(std::move(context_lost_callback)),
      vk_context_provider(vulkan_context_provider),
#if BUILDFLAG(ENABLE_VULKAN)
      gr_context(vk_context_provider ? vk_context_provider->GetGrContext()
                                     : nullptr),
#endif
      use_vulkan_gr_context(!!gr_context),
      share_group_(std::move(share_group)),
      context_(context),
      real_context_(std::move(context)),
      surface_(std::move(surface)),
      weak_ptr_factory_(this) {
  if (use_vulkan_gr_context)
    use_virtualized_gl_contexts = false;
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "RasterDecoderContextState", base::ThreadTaskRunnerHandle::Get());
  }
  // Initialize the scratch buffer to some small initial size.
  scratch_deserialization_buffer_.resize(
      kInitialScratchDeserializationBufferSize);
}

RasterDecoderContextState::~RasterDecoderContextState() {
  if (gr_context)
    gr_context->abandonContext();
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void RasterDecoderContextState::InitializeGrContext(
    const GpuDriverBugWorkarounds& workarounds,
    GrContextOptions::PersistentCache* cache,
    GpuProcessActivityFlags* activity_flags,
    gl::ProgressReporter* progress_reporter) {
  if (!use_vulkan_gr_context) {
    DCHECK(context_->IsCurrent(nullptr));
    sk_sp<GrGLInterface> interface(gl::init::CreateGrGLInterface(
        *context_->GetVersionInfo(), workarounds.use_es2_for_oopr,
        progress_reporter));
    if (!interface) {
      LOG(ERROR) << "OOP raster support disabled: GrGLInterface creation "
                    "failed.";
      return;
    }

    if (activity_flags && cache) {
      // |activity_flags| is safe to capture here since it must outlive the
      // this context state.
      interface->fFunctions.fProgramBinary =
          [activity_flags](GrGLuint program, GrGLenum binaryFormat,
                           void* binary, GrGLsizei length) {
            GpuProcessActivityFlags::ScopedSetFlag scoped_set_flag(
                activity_flags, ActivityFlagsBase::FLAG_LOADING_PROGRAM_BINARY);
            glProgramBinary(program, binaryFormat, binary, length);
          };
    }

    // If you make any changes to the GrContext::Options here that could
    // affect text rendering, make sure to match the capabilities initialized
    // in GetCapabilities and ensuring these are also used by the
    // PaintOpBufferSerializer.
    GrContextOptions options;
    options.fDriverBugWorkarounds =
        GrDriverBugWorkarounds(workarounds.ToIntSet());
    options.fDisableCoverageCountingPaths = true;
    size_t max_resource_cache_bytes = 0u;
    raster::DetermineGrCacheLimitsFromAvailableMemory(
        &max_resource_cache_bytes, &glyph_cache_max_texture_bytes);
    options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;
    options.fPersistentCache = cache;
    options.fAvoidStencilBuffers = workarounds.avoid_stencil_buffers;
    owned_gr_context = GrContext::MakeGL(std::move(interface), options);
    gr_context = owned_gr_context.get();
    if (!gr_context) {
      LOG(ERROR) << "OOP raster support disabled: GrContext creation "
                    "failed.";
    } else {
      constexpr int kMaxGaneshResourceCacheCount = 16384;
      gr_context->setResourceCacheLimits(kMaxGaneshResourceCacheCount,
                                         max_resource_cache_bytes);
    }
  }
  transfer_cache = std::make_unique<ServiceTransferCache>();
}

bool RasterDecoderContextState::InitializeGL(
    scoped_refptr<gles2::FeatureInfo> feature_info) {
  // We still need initialize GL when Vulkan is used, because RasterDecoder
  // depends on GL.
  // TODO(penghuang): don't initialize GL when RasterDecoder can work without
  // GL.
  if (IsGLInitialized()) {
    DCHECK(feature_info == feature_info_);
    DCHECK(context_state_);
    return true;
  }

  DCHECK(context_->IsCurrent(nullptr));

  feature_info_ = std::move(feature_info);
  feature_info_->Initialize(gpu::CONTEXT_TYPE_OPENGLES2,
                            false /* is_passthrough_cmd_decoder */,
                            gles2::DisallowedFeatures());

  auto* api = gl::g_current_gl_context;
  const GLint kGLES2RequiredMinimumVertexAttribs = 8u;
  GLint max_vertex_attribs = 0;
  api->glGetIntegervFn(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attribs);
  if (max_vertex_attribs < kGLES2RequiredMinimumVertexAttribs) {
    feature_info_ = nullptr;
    return false;
  }

  context_state_ = std::make_unique<gles2::ContextState>(
      feature_info_.get(), false /* track_texture_and_sampler_units */);

  context_state_->set_api(api);
  context_state_->InitGenericAttribs(max_vertex_attribs);

  // Set all the default state because some GL drivers get it wrong.
  // TODO(backer): Not all of this state needs to be initialized. Reduce the set
  // if perf becomes a problem.
  context_state_->InitCapabilities(nullptr);
  context_state_->InitState(nullptr);

  if (use_virtualized_gl_contexts) {
    auto virtual_context = base::MakeRefCounted<GLContextVirtual>(
        share_group_.get(), real_context_.get(),
        weak_ptr_factory_.GetWeakPtr());
    if (!virtual_context->Initialize(surface_.get(), gl::GLContextAttribs())) {
      feature_info_ = nullptr;
      context_state_ = nullptr;
      return false;
    }
    context_ = std::move(virtual_context);
    MakeCurrent(nullptr);
  }
  return true;
}

bool RasterDecoderContextState::MakeCurrent(gl::GLSurface* surface) {
  if (use_vulkan_gr_context)
    return true;

  if (context_lost_)
    return false;

  if (!context_->MakeCurrent(surface ? surface : surface_.get())) {
    MarkContextLost();
    return false;
  }
  return true;
}

void RasterDecoderContextState::MarkContextLost() {
  if (!context_lost_) {
    context_lost_ = true;
    // context_state_ could be nullptr for some unittests.
    if (context_state_)
      context_state_->MarkContextLost();
    if (gr_context)
      gr_context->abandonContext();
    std::move(context_lost_callback).Run();
  }
}

bool RasterDecoderContextState::IsCurrent(gl::GLSurface* surface) {
  if (use_vulkan_gr_context)
    return true;
  return context_->IsCurrent(surface);
}

bool RasterDecoderContextState::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (gr_context)
    DumpGrMemoryStatistics(gr_context, pmd, base::nullopt);
  return true;
}

void RasterDecoderContextState::PurgeMemory(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  if (!gr_context) {
    DCHECK(!transfer_cache);
    return;
  }

  // Ensure the context is current before doing any GPU cleanup.
  MakeCurrent(nullptr);

  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      // This function is only called with moderate or critical pressure.
      NOTREACHED();
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      // With moderate pressure, clear any unlocked resources.
      gr_context->purgeUnlockedResources(true /* scratchResourcesOnly */);
      scratch_deserialization_buffer_.resize(
          kInitialScratchDeserializationBufferSize);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // With critical pressure, purge as much as possible.
      gr_context->freeGpuResources();
      scratch_deserialization_buffer_.resize(0u);
      scratch_deserialization_buffer_.shrink_to_fit();
      break;
  }

  transfer_cache->PurgeMemory(memory_pressure_level);
}

void RasterDecoderContextState::PessimisticallyResetGrContext() const {
  // Calling GrContext::resetContext() is very cheap, so we do it
  // pessimistically. We could dirty less state if skia state setting
  // performance becomes an issue.
  if (gr_context && !use_vulkan_gr_context)
    gr_context->resetContext();
}

bool RasterDecoderContextState::initialized() const {
  return true;
}

const gles2::ContextState* RasterDecoderContextState::GetContextState() {
  if (need_context_state_reset) {
    // Returning nullptr to force full state restoration by the caller.  We do
    // this because GrContext changes to GL state are untracked in our
    // context_state_.
    return nullptr;
  }
  return context_state_.get();
}

void RasterDecoderContextState::RestoreState(
    const gles2::ContextState* prev_state) {
  PessimisticallyResetGrContext();
  context_state_->RestoreState(prev_state);
  need_context_state_reset = false;
}

void RasterDecoderContextState::RestoreGlobalState() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreGlobalState(nullptr);
}
void RasterDecoderContextState::ClearAllAttributes() const {}

void RasterDecoderContextState::RestoreActiveTexture() const {
  PessimisticallyResetGrContext();
}

void RasterDecoderContextState::RestoreAllTextureUnitAndSamplerBindings(
    const gles2::ContextState* prev_state) const {
  PessimisticallyResetGrContext();
}

void RasterDecoderContextState::RestoreActiveTextureUnitBinding(
    unsigned int target) const {
  PessimisticallyResetGrContext();
}

void RasterDecoderContextState::RestoreBufferBinding(unsigned int target) {
  PessimisticallyResetGrContext();
  if (target == GL_PIXEL_PACK_BUFFER) {
    context_state_->UpdatePackParameters();
  } else if (target == GL_PIXEL_UNPACK_BUFFER) {
    context_state_->UpdateUnpackParameters();
  }
  context_state_->api()->glBindBufferFn(target, 0);
}

void RasterDecoderContextState::RestoreBufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreBufferBindings();
}

void RasterDecoderContextState::RestoreFramebufferBindings() const {
  PessimisticallyResetGrContext();
  context_state_->fbo_binding_for_scissor_workaround_dirty = true;
  context_state_->stencil_state_changed_since_validation = true;
}

void RasterDecoderContextState::RestoreRenderbufferBindings() {
  PessimisticallyResetGrContext();
  context_state_->RestoreRenderbufferBindings();
}

void RasterDecoderContextState::RestoreProgramBindings() const {
  PessimisticallyResetGrContext();
  context_state_->RestoreProgramSettings(nullptr, false);
}

void RasterDecoderContextState::RestoreTextureUnitBindings(
    unsigned unit) const {
  PessimisticallyResetGrContext();
}

void RasterDecoderContextState::RestoreVertexAttribArray(unsigned index) {
  NOTIMPLEMENTED();
}

void RasterDecoderContextState::RestoreAllExternalTextureBindingsIfNeeded() {
  PessimisticallyResetGrContext();
}

QueryManager* RasterDecoderContextState::GetQueryManager() {
  return nullptr;
}

}  // namespace raster
}  // namespace gpu
