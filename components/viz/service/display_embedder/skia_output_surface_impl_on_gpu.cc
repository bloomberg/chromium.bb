// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_on_gpu.h"

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/gl_renderer_copier.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/texture_deleter.h"
#include "components/viz/service/display_embedder/direct_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gr_shader_cache.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/command_buffer_task_executor.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "gpu/vulkan/buildflags.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/private/SkDeferredDisplayList.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"
#endif

#if BUILDFLAG(ENABLE_VULKAN) && defined(USE_X11)
#include "components/viz/service/display_embedder/skia_output_device_x11.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

namespace viz {

// Skia gr_context() and |context_provider_| share an underlying GLContext.
// Each of them caches some GL state. Interleaving usage could make cached
// state inconsistent with GL state. Using a ScopedUseContextProvider whenever
// |context_provider_| could be accessed (e.g. processing completed queries),
// will keep cached state consistent with driver GL state.
class SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider {
 public:
  ScopedUseContextProvider(SkiaOutputSurfaceImplOnGpu* impl_on_gpu,
                           GLuint texture_client_id)
      : impl_on_gpu_(impl_on_gpu) {
    if (!impl_on_gpu_->MakeCurrent(true /* need_fbo0 */)) {
      valid_ = false;
      return;
    }

    // GLRendererCopier uses context_provider_->ContextGL(), which caches GL
    // state and removes state setting calls that it considers redundant. To get
    // to a known GL state, we first set driver GL state and then make client
    // side consistent with that.
    auto* api = impl_on_gpu_->api_;
    api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
    impl_on_gpu_->context_provider_->SetGLRendererCopierRequiredState(
        texture_client_id);
  }

  ~ScopedUseContextProvider() {
    if (valid_)
      impl_on_gpu_->gr_context()->resetContext();
  }

  bool valid() { return valid_; }

 private:
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;
  bool valid_ = true;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseContextProvider);
};

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    GpuServiceImpl* gpu_service) {
  auto* channel_manager = gpu_service->gpu_channel_manager();
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info());
}

scoped_refptr<gpu::gles2::FeatureInfo> CreateFeatureInfo(
    gpu::CommandBufferTaskExecutor* task_executor) {
  return base::MakeRefCounted<gpu::gles2::FeatureInfo>(
      gpu::GpuDriverBugWorkarounds(
          task_executor->gpu_feature_info().enabled_gpu_driver_bug_workarounds),
      task_executor->gpu_feature_info());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    GpuServiceImpl* gpu_service) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return gpu_service->sync_point_manager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      gpu_service->skia_output_surface_sequence_id());
}

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::CommandBufferTaskExecutor* task_executor,
    gpu::SequenceId sequence_id) {
  auto command_buffer_id = gpu::CommandBufferId::FromUnsafeValue(
      g_next_command_buffer_id.GetNext() + 1);
  return task_executor->sync_point_manager()->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE, command_buffer_id,
      sequence_id);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(GpuServiceImpl* gpu_service) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      gpu_service->shared_image_manager(), nullptr);
}

std::unique_ptr<gpu::SharedImageRepresentationFactory>
CreateSharedImageRepresentationFactory(
    gpu::CommandBufferTaskExecutor* task_executor) {
  // TODO(https://crbug.com/899905): Use a real MemoryTracker, not nullptr.
  return std::make_unique<gpu::SharedImageRepresentationFactory>(
      task_executor->shared_image_manager(), nullptr);
}

class ScopedSurfaceToTexture {
 public:
  ScopedSurfaceToTexture(scoped_refptr<DirectContextProvider> context_provider,
                         SkSurface* surface)
      : context_provider_(context_provider) {
    GrBackendTexture skia_texture =
        surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
    GrGLTextureInfo gl_texture_info;
    skia_texture.getGLTextureInfo(&gl_texture_info);
    GLuint client_id = context_provider_->GenClientTextureId();
    auto* texture_manager = context_provider_->texture_manager();
    texture_ref_ =
        texture_manager->CreateTexture(client_id, gl_texture_info.fID);
    texture_manager->SetTarget(texture_ref_.get(), gl_texture_info.fTarget);
    texture_manager->SetLevelInfo(
        texture_ref_.get(), gl_texture_info.fTarget,
        /*level=*/0,
        /*internal_format=*/GL_RGBA, surface->width(), surface->height(),
        /*depth=*/1, /*border=*/0,
        /*format=*/GL_RGBA, /*type=*/GL_UNSIGNED_BYTE,
        /*cleared_rect=*/gfx::Rect(surface->width(), surface->height()));
  }

  ~ScopedSurfaceToTexture() {
    context_provider_->DeleteClientTextureId(client_id());

    // Skia owns the texture. It will delete it when it is done.
    texture_ref_->ForceContextLost();
  }

  GLuint client_id() { return texture_ref_->client_id(); }

 private:
  scoped_refptr<DirectContextProvider> context_provider_;
  scoped_refptr<gpu::gles2::TextureRef> texture_ref_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSurfaceToTexture);
};

// This SingleThreadTaskRunner runs tasks on the GPU main thread, where
// DirectContextProvider can safely service calls. It wraps all posted tasks to
// ensure that |impl_on_gpu_->context_provider_| is made current and in a known
// state when the task is run. If |impl_on_gpu| is destructed, pending tasks are
// no-oped when they are run.
class ContextCurrentTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit ContextCurrentTaskRunner(SkiaOutputSurfaceImplOnGpu* impl_on_gpu)
      : real_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        impl_on_gpu_(impl_on_gpu) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    return real_task_runner_->PostDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return real_task_runner_->PostNonNestableDelayedTask(
        from_here, WrapClosure(std::move(task)), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return real_task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  base::OnceClosure WrapClosure(base::OnceClosure task) {
    return base::BindOnce(
        [](base::WeakPtr<SkiaOutputSurfaceImplOnGpu> impl_on_gpu,
           base::OnceClosure task) {
          if (!impl_on_gpu)
            return;
          SkiaOutputSurfaceImplOnGpu::ScopedUseContextProvider scoped_use(
              impl_on_gpu.get(), /*texture_client_id=*/0);
          if (!scoped_use.valid())
            return;

          std::move(task).Run();
        },
        impl_on_gpu_->weak_ptr(), std::move(task));
  }

  ~ContextCurrentTaskRunner() override = default;

  scoped_refptr<base::SingleThreadTaskRunner> real_task_runner_;
  SkiaOutputSurfaceImplOnGpu* const impl_on_gpu_;

  DISALLOW_COPY_AND_ASSIGN(ContextCurrentTaskRunner);
};

}  // namespace

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::~OffscreenSurface() = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface::OffscreenSurface(
    OffscreenSurface&& offscreen_surface) = default;

SkiaOutputSurfaceImplOnGpu::OffscreenSurface&
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::operator=(
    OffscreenSurface&& offscreen_surface) = default;

SkSurface* SkiaOutputSurfaceImplOnGpu::OffscreenSurface::surface() const {
  return surface_.get();
}

sk_sp<SkPromiseImageTexture>
SkiaOutputSurfaceImplOnGpu::OffscreenSurface::fulfill() {
  DCHECK(surface_);
  if (!promise_texture_) {
    promise_texture_ = SkPromiseImageTexture::Make(
        surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess));
  }
  return promise_texture_;
}

void SkiaOutputSurfaceImplOnGpu::OffscreenSurface::set_surface(
    sk_sp<SkSurface> surface) {
  surface_ = std::move(surface);
  promise_texture_ = {};
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    gpu::SurfaceHandle surface_handle,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    gpu::MailboxManager* mailbox_manager,
    scoped_refptr<gpu::SyncPointClientState> sync_point_client_state,
    std::unique_ptr<gpu::SharedImageRepresentationFactory> sir_factory,
    gpu::raster::GrShaderCache* gr_shader_cache,
    VulkanContextProvider* vulkan_context_provider,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : surface_handle_(surface_handle),
      feature_info_(std::move(feature_info)),
      mailbox_manager_(mailbox_manager),
      sync_point_client_state_(std::move(sync_point_client_state)),
      shared_image_representation_factory_(std::move(sir_factory)),
      gr_shader_cache_(gr_shader_cache),
      vulkan_context_provider_(vulkan_context_provider),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback),
      buffer_presented_callback_(buffer_presented_callback),
      context_lost_callback_(context_lost_callback),
      weak_ptr_factory_(this) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    GpuServiceImpl* gpu_service,
    gpu::SurfaceHandle surface_handle,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : SkiaOutputSurfaceImplOnGpu(
          surface_handle,
          CreateFeatureInfo(gpu_service),
          gpu_service->mailbox_manager(),
          CreateSyncPointClientState(gpu_service),
          CreateSharedImageRepresentationFactory(gpu_service),
          gpu_service->gr_shader_cache(),
          gpu_service->vulkan_context_provider(),
          did_swap_buffer_complete_callback,
          buffer_presented_callback,
          context_lost_callback) {
#if defined(USE_OZONE)
  window_surface_ = ui::OzonePlatform::GetInstance()
                        ->GetSurfaceFactoryOzone()
                        ->CreatePlatformWindowSurface(surface_handle);
#endif
  if (gpu_service) {
    gpu_preferences_ = gpu_service->gpu_channel_manager()->gpu_preferences();
  } else {
    auto* command_line = base::CommandLine::ForCurrentProcess();
    gpu_preferences_ = gpu::gles2::ParseGpuPreferences(command_line);
  }

  if (is_using_vulkan())
    InitializeForVulkan(gpu_service);
  else
    InitializeForGLWithGpuService(gpu_service);
}

SkiaOutputSurfaceImplOnGpu::SkiaOutputSurfaceImplOnGpu(
    gpu::CommandBufferTaskExecutor* task_executor,
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::SequenceId sequence_id,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
    const BufferPresentedCallback& buffer_presented_callback,
    const ContextLostCallback& context_lost_callback)
    : SkiaOutputSurfaceImplOnGpu(
          gpu::kNullSurfaceHandle,
          CreateFeatureInfo(task_executor),
          task_executor->mailbox_manager(),
          CreateSyncPointClientState(task_executor, sequence_id),
          CreateSharedImageRepresentationFactory(task_executor),
          nullptr /* gr_shader_cache */,
          nullptr /* vulkan_context_provider */,
          did_swap_buffer_complete_callback,
          buffer_presented_callback,
          context_lost_callback) {
  DCHECK(!is_using_vulkan());
  gl_surface_ = std::move(gl_surface);
  context_state_ = std::move(shared_context_state);
  InitializeForGL();
}

SkiaOutputSurfaceImplOnGpu::~SkiaOutputSurfaceImplOnGpu() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // |context_provider_| and clients want either the context to be lost or made
  // current on destruction.
  if (MakeCurrent(false /* need_fbo0 */)) {
    // This ensures any outstanding callbacks for promise images are performed.
    gr_context()->flush();
  }
  copier_ = nullptr;
  texture_deleter_ = nullptr;
  context_provider_ = nullptr;

  sync_point_client_state_->Destroy();
}

void SkiaOutputSurfaceImplOnGpu::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(gr_context());

  base::ScopedClosureRunner scoped_runner;
  if (event) {
    scoped_runner.ReplaceClosure(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!is_using_vulkan()) {
    if (!MakeCurrent(surface_handle_ /* need_fbo0 */))
      return;
    size_ = size;
    color_space_ = color_space;
    if (output_device_) {
      output_device_->Reshape(size_);
      sk_surface_ = output_device_->DrawSurface();
    } else {
      // Conversion to GLSurface's color space follows the same logic as in
      // gl::GetGLColorSpace().
      gl::GLSurface::ColorSpace surface_color_space =
          color_space.IsHDR() ? gl::GLSurface::ColorSpace::SCRGB_LINEAR
                              : gl::GLSurface::ColorSpace::UNSPECIFIED;
      if (!gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                               has_alpha)) {
        LOG(FATAL) << "Failed to resize.";
        // TODO(penghuang): Handle the failure.
      }
      CreateSkSurfaceForGL();
    }
  } else {
#if BUILDFLAG(ENABLE_VULKAN)
    if (!output_device_) {
      if (surface_handle_ == gpu::kNullSurfaceHandle) {
        output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
            gr_context(), false /* flipped */, false /* has_alpha */);
      } else {
        output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
            vulkan_context_provider_, surface_handle_);
      }
    }
    output_device_->Reshape(size);
    sk_surface_ = output_device_->DrawSurface();
#else
    NOTREACHED();
#endif
  }

  if (characterization) {
    sk_surface_->characterize(characterization);
    DCHECK(characterization->isValid());
  }
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintCurrentFrame(
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::unique_ptr<SkDeferredDisplayList> overdraw_ddl,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);
  DCHECK(sk_surface_);

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));

  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }
    sk_surface_->draw(ddl.get());
    gr_context()->flush();
  }

  // Note that the ScopedCacheUse for GrShaderCache is scoped until the
  // ReleaseFenceSync call here since releasing the fence may schedule a
  // different decoder's stream which also uses the shader cache.
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);

  if (overdraw_ddl) {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_) {
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    }

    sk_sp<SkSurface> overdraw_surface = SkSurface::MakeRenderTarget(
        gr_context(), overdraw_ddl->characterization(), SkBudgeted::kNo);
    overdraw_surface->draw(overdraw_ddl.get());

    SkPaint paint;
    sk_sp<SkImage> overdraw_image = overdraw_surface->makeImageSnapshot();

    sk_sp<SkColorFilter> colorFilter = SkiaHelper::MakeOverdrawColorFilter();
    paint.setColorFilter(colorFilter);
    // TODO(xing.xu): move below to the thread where skia record happens.
    sk_surface_->getCanvas()->drawImage(overdraw_image.get(), 0, 0, &paint);
    gr_context()->flush();
  }
}

void SkiaOutputSurfaceImplOnGpu::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sk_surface_);
  if (!MakeCurrent(surface_handle_ /* need_fbo0 */))
    return;

  base::TimeTicks swap_start = base::TimeTicks::Now();
  OnSwapBuffers();

  base::TimeTicks swap_end;
  if (output_device_) {
    gpu::SwapBuffersCompleteParams params;
    params.swap_response.swap_start = swap_start;
    if (capabilities_.supports_post_sub_buffer) {
      DCHECK(frame.sub_buffer_rect);
      DCHECK(!frame.sub_buffer_rect->IsEmpty());
      params.swap_response.result =
          output_device_->PostSubBuffer(*frame.sub_buffer_rect);
    } else {
      params.swap_response.result = output_device_->SwapBuffers();
    }
    swap_end = params.swap_response.swap_end = base::TimeTicks::Now();
    sk_surface_ = output_device_->DrawSurface();
    DidSwapBuffersComplete(params);
    buffer_presented_callback_.Run(gfx::PresentationFeedback(
        params.swap_response.swap_end, base::TimeDelta(), 0 /* flag */));

  } else {
    DCHECK(!is_using_vulkan());
    gl_surface_->SwapBuffers(buffer_presented_callback_);
    swap_end = base::TimeTicks::Now();
  }
  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, swap_start, 1);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, swap_end, 1);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(frame.latency_info);
}

void SkiaOutputSurfaceImplOnGpu::FinishPaintRenderPass(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<gpu::SyncToken> sync_tokens,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(ddl);

  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  PullTextureUpdates(std::move(sync_tokens));

  auto& offscreen = offscreen_surfaces_[id];
  SkSurfaceCharacterization characterization;
  // TODO(penghuang): Using characterization != ddl->characterization(), when
  // the SkSurfaceCharacterization::operator!= is implemented in Skia.
  if (!offscreen.surface() ||
      !offscreen.surface()->characterize(&characterization) ||
      characterization != ddl->characterization()) {
    offscreen.set_surface(SkSurface::MakeRenderTarget(
        gr_context(), ddl->characterization(), SkBudgeted::kNo));
    DCHECK(offscreen.surface());
  }
  {
    base::Optional<gpu::raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache_)
      cache_use.emplace(gr_shader_cache_, gpu::kInProcessCommandBufferClientId);
    offscreen.surface()->draw(ddl.get());
    gr_context()->flush();
  }
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());
  for (const auto& id : ids) {
    auto it = offscreen_surfaces_.find(id);
    DCHECK(it != offscreen_surfaces_.end());
    offscreen_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplOnGpu::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(crbug.com/898595): Do this on the GPU instead of CPU with Vulkan.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  bool from_fbo0 = !id;
  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  DCHECK(from_fbo0 ||
         offscreen_surfaces_.find(id) != offscreen_surfaces_.end());
  auto* surface =
      from_fbo0 ? sk_surface_.get() : offscreen_surfaces_[id].surface();

  if (!is_using_vulkan()) {
    // Lazy initialize GLRendererCopier.
    if (!copier_) {
      context_provider_ = base::MakeRefCounted<DirectContextProvider>(
          context_state_->context(), gl_surface_, supports_alpha_,
          gpu_preferences_, feature_info_.get());
      auto result = context_provider_->BindToCurrentThread();
      if (result != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Couldn't initialize GLRendererCopier";
        context_provider_ = nullptr;
        return;
      }
      context_current_task_runner_ =
          base::MakeRefCounted<ContextCurrentTaskRunner>(this);
      texture_deleter_ =
          std::make_unique<TextureDeleter>(context_current_task_runner_);
      copier_ = std::make_unique<GLRendererCopier>(context_provider_,
                                                   texture_deleter_.get());
      copier_->set_async_gl_task_runner(context_current_task_runner_);
    }
    surface->flush();

    GLuint gl_id = 0;
    GLenum internal_format = supports_alpha_ ? GL_RGBA : GL_RGB;
    bool flipped = from_fbo0 ? !capabilities_.flipped_output_surface : false;

    base::Optional<ScopedSurfaceToTexture> texture_mapper;
    if (!from_fbo0 || output_device_) {
      texture_mapper.emplace(context_provider_.get(), surface);
      gl_id = texture_mapper.value().client_id();
      internal_format = GL_RGBA;
    }
    gfx::Size surface_size(surface->width(), surface->height());
    ScopedUseContextProvider use_context_provider(this, gl_id);
    copier_->CopyFromTextureOrFramebuffer(std::move(request), geometry,
                                          internal_format, gl_id, surface_size,
                                          flipped, color_space);

    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries())
      ScheduleDelayedWork();

    return;
  }

  SkBitmap bitmap;
  if (request->is_scaled()) {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.sampling_bounds.width(), geometry.sampling_bounds.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.sampling_bounds.x(),
                        geometry.sampling_bounds.y());

    // Execute the scaling: For downscaling, use the RESIZE_BETTER strategy
    // (appropriate for thumbnailing); and, for upscaling, use the RESIZE_BEST
    // strategy. Note that processing is only done on the subset of the
    // RenderPass output that contributes to the result.
    using skia::ImageOperations;
    const bool is_downscale_in_both_dimensions =
        request->scale_to().x() < request->scale_from().x() &&
        request->scale_to().y() < request->scale_from().y();
    const ImageOperations::ResizeMethod method =
        is_downscale_in_both_dimensions ? ImageOperations::RESIZE_BETTER
                                        : ImageOperations::RESIZE_BEST;
    bitmap = ImageOperations::Resize(
        bitmap, method, geometry.result_bounds.width(),
        geometry.result_bounds.height(),
        SkIRect{geometry.result_selection.x(), geometry.result_selection.y(),
                geometry.result_selection.right(),
                geometry.result_selection.bottom()});
  } else {
    SkImageInfo sampling_bounds_info = SkImageInfo::Make(
        geometry.result_selection.width(), geometry.result_selection.height(),
        SkColorType::kN32_SkColorType, SkAlphaType::kPremul_SkAlphaType,
        surface->getCanvas()->imageInfo().refColorSpace());
    bitmap.allocPixels(sampling_bounds_info);
    surface->readPixels(bitmap, geometry.readback_offset.x(),
                        geometry.readback_offset.y());
  }

  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to the
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());

  // Deliver the result. SkiaRenderer supports RGBA_BITMAP and I420_PLANES
  // only. For legacy reasons, if a RGBA_TEXTURE request is being made, clients
  // are prepared to accept RGBA_BITMAP results.
  //
  // TODO(crbug/754872): Get rid of the legacy behavior and send empty results
  // for RGBA_TEXTURE requests once tab capture is moved into VIZ.
  const CopyOutputResult::Format result_format =
      (request->result_format() == CopyOutputResult::Format::RGBA_TEXTURE)
          ? CopyOutputResult::Format::RGBA_BITMAP
          : request->result_format();
  // Note: The CopyOutputSkBitmapResult automatically provides I420 format
  // conversion, if needed.
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      result_format, geometry.result_selection, bitmap));
}

gpu::DecoderContext* SkiaOutputSurfaceImplOnGpu::decoder() {
  return context_provider_->decoder();
}

void SkiaOutputSurfaceImplOnGpu::ScheduleDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delayed_work_pending_)
    return;
  delayed_work_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImplOnGpu::PerformDelayedWork,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(2));
}

void SkiaOutputSurfaceImplOnGpu::PerformDelayedWork() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ScopedUseContextProvider use_context_provider(this, /*texture_client_id=*/0);

  delayed_work_pending_ = false;
  if (MakeCurrent(true /* need_fbo0 */)) {
    decoder()->PerformIdleWork();
    decoder()->ProcessPendingQueries(false);
    if (decoder()->HasMoreIdleWork() || decoder()->HasPendingQueries()) {
      ScheduleDelayedWork();
    }
  }
}

sk_sp<SkPromiseImageTexture> SkiaOutputSurfaceImplOnGpu::FulfillPromiseTexture(
    const gpu::MailboxHolder& mailbox_holder,
    const gfx::Size& size,
    const ResourceFormat resource_format,
    std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!*shared_image_out && mailbox_holder.mailbox.IsSharedImage()) {
    std::unique_ptr<gpu::SharedImageRepresentationSkia> shared_image =
        shared_image_representation_factory_->ProduceSkia(
            mailbox_holder.mailbox);
    if (!shared_image) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "mailbox not found in SharedImageManager.";
      return nullptr;
    }
    if (!(shared_image->usage() & gpu::SHARED_IMAGE_USAGE_DISPLAY)) {
      DLOG(ERROR) << "Failed to fulfill the promise texture - SharedImage "
                     "was not created with display usage.";
      return nullptr;
    }
    *shared_image_out = std::move(shared_image);
  }
  if (*shared_image_out) {
    auto promise_texture =
        (*shared_image_out)->BeginReadAccess(sk_surface_.get());
    DLOG_IF(ERROR, !promise_texture)
        << "Failed to begin read access for SharedImageRepresentationSkia";
    return promise_texture;
  }

  if (is_using_vulkan()) {
    // Probably this texture is created with wrong inteface (GLES2Interface).
    DLOG(ERROR) << "Failed to fulfill the promise texture whose backend is not "
                   "compitable with vulkan.";
    return nullptr;
  }

  auto* texture_base = mailbox_manager_->ConsumeTexture(mailbox_holder.mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    return nullptr;
  }
  BindOrCopyTextureIfNecessary(texture_base);
  GrBackendTexture backend_texture;
  gpu::GetGrBackendTexture(gl_version_info_, texture_base->target(), size,
                           texture_base->service_id(), resource_format,
                           &backend_texture);
  if (!backend_texture.isValid()) {
    DLOG(ERROR) << "Failed to fulfill the promise texture.";
    return nullptr;
  }
  return SkPromiseImageTexture::Make(backend_texture);
}

sk_sp<SkPromiseImageTexture> SkiaOutputSurfaceImplOnGpu::FulfillPromiseTexture(
    const RenderPassId id,
    std::unique_ptr<gpu::SharedImageRepresentationSkia>* shared_image_out) {
  DCHECK(!*shared_image_out);
  auto it = offscreen_surfaces_.find(id);
  DCHECK(it != offscreen_surfaces_.end());
  auto promise_texture = it->second.fulfill();
  if (!promise_texture) {
    DLOG(ERROR)
        << "Failed to fulfill the promise texture created from RenderPassId:"
        << id;
    return nullptr;
  }
  return promise_texture;
}

sk_sp<GrContextThreadSafeProxy>
SkiaOutputSurfaceImplOnGpu::GetGrContextThreadSafeProxy() {
  return gr_context()->threadSafeProxy();
}

void SkiaOutputSurfaceImplOnGpu::DestroySkImages(
    std::vector<sk_sp<SkImage>>&& images,
    uint64_t sync_fence_release) {
  DCHECK(!images.empty());
  // The window could be destroyed already, and the MakeCurrent will fail with
  // an destroyed window, so MakeCurrent without requiring the fbo0.
  MakeCurrent(false /* need_fbo0 */);
#if DCHECK_IS_ON()
  for (const auto& image : images)
    DCHECK(image->unique());
#endif
  images.clear();
  // Flush the gr_context() to make sure images are released, and the release
  // and done callbacks are called.
  gr_context()->flush();
  ReleaseFenceSyncAndPushTextureUpdates(sync_fence_release);
}

void SkiaOutputSurfaceImplOnGpu::SetCapabilitiesForTesting(
    const OutputSurface::Capabilities& capabilities) {
  MakeCurrent(false /* need_fbo0 */);
  // Check that we're using an offscreen surface.
  DCHECK(output_device_);
  capabilities_ = capabilities;
  output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
      gr_context(), capabilities_.flipped_output_surface,
      false /* has_alpha */);
}

#if defined(OS_WIN)
void SkiaOutputSurfaceImplOnGpu::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}
#endif

void SkiaOutputSurfaceImplOnGpu::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  params.swap_response.swap_id = pending_swap_completed_params_.front().first;
  gfx::Size pixel_size = pending_swap_completed_params_.front().second;
  pending_swap_completed_params_.pop_front();
  did_swap_buffer_complete_callback_.Run(params, pixel_size);
}

const gpu::gles2::FeatureInfo* SkiaOutputSurfaceImplOnGpu::GetFeatureInfo()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return feature_info_.get();
}

const gpu::GpuPreferences& SkiaOutputSurfaceImplOnGpu::GetGpuPreferences()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return gpu_preferences_;
}

void SkiaOutputSurfaceImplOnGpu::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaOutputSurfaceImplOnGpu::AddFilter(IPC::MessageFilter* message_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

int32_t SkiaOutputSurfaceImplOnGpu::GetRouteID() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGL() {
  if (!MakeCurrent(true /* need_fbo0 */))
    return;

  auto* context = context_state_->real_context();
  auto* current_gl = context->GetCurrentGL();
  api_ = current_gl->Api;
  gl_version_info_ = context->GetVersionInfo();

  if (surface_handle_) {
    capabilities_.flipped_output_surface = gl_surface_->FlipsVertically();

    // Get alpha and stencil bits from the default frame buffer.
    api_->glBindFramebufferEXTFn(GL_FRAMEBUFFER, 0);
    gr_context()->resetContext(kRenderTarget_GrGLBackendState);
    const auto* version = current_gl->Version;
    GLint stencil_bits = 0;
    GLint alpha_bits = 0;
    if (version->is_desktop_core_profile) {
      api_->glGetFramebufferAttachmentParameterivEXTFn(
          GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
          &stencil_bits);
      api_->glGetFramebufferAttachmentParameterivEXTFn(
          GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
          &alpha_bits);
    } else {
      api_->glGetIntegervFn(GL_STENCIL_BITS, &stencil_bits);
      api_->glGetIntegervFn(GL_ALPHA_BITS, &alpha_bits);
    }
    CHECK_GL_ERROR();
    capabilities_.supports_stencil = stencil_bits > 0;
    supports_alpha_ = alpha_bits > 0;
  }
}

void SkiaOutputSurfaceImplOnGpu::InitializeForGLWithGpuService(
    GpuServiceImpl* gpu_service) {
  if (surface_handle_) {
    gl_surface_ = gpu::ImageTransportSurface::CreateNativeSurface(
        weak_ptr_factory_.GetWeakPtr(), surface_handle_, gl::GLSurfaceFormat());
  } else {
    gl_surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  }
  DCHECK(gl_surface_);

  context_state_ = gpu_service->GetContextStateForGLSurface(gl_surface_.get());
  if (!context_state_) {
    LOG(FATAL) << "Failed to create GrContext";
    // TODO(penghuang): handle the failure.
  }
  if (!surface_handle_) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), true /* flipped */, false /* has_alpha */);
    capabilities_.flipped_output_surface = true;
    capabilities_.supports_stencil = false;
    supports_alpha_ = false;
  }
  InitializeForGL();
}

void SkiaOutputSurfaceImplOnGpu::InitializeForVulkan(
    GpuServiceImpl* gpu_service) {
  context_state_ = gpu_service->GetContextStateForVulkan();
  DCHECK(context_state_);
  supports_alpha_ = true;
  capabilities_.flipped_output_surface = true;
#if BUILDFLAG(ENABLE_VULKAN)
  if (surface_handle_ == gpu::kNullSurfaceHandle) {
    output_device_ = std::make_unique<SkiaOutputDeviceOffscreen>(
        gr_context(), false /* flipped */, false /* has_alpha */);
  } else {
#if defined(USE_X11)
    if (gpu_preferences_.disable_vulkan_surface) {
      output_device_ =
          std::make_unique<SkiaOutputDeviceX11>(gr_context(), surface_handle_);
    } else {
      output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
          vulkan_context_provider_, surface_handle_);
    }
#else
    output_device_ = std::make_unique<SkiaOutputDeviceVulkan>(
        vulkan_context_provider_, surface_handle_);
#endif
  }
  capabilities_.supports_post_sub_buffer =
      output_device_->SupportPostSubBuffer();
#endif
}

void SkiaOutputSurfaceImplOnGpu::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (texture_base->GetType() != gpu::TextureBase::Type::kValidated)
    return;
  // If a texture is validated and bound to an image, we may defer copying the
  // image to the texture until the texture is used. It is for implementing low
  // latency drawing (e.g. fast ink) and avoiding unnecessary texture copy. So
  // we need check the texture image state, and bind or copy the image to the
  // texture if necessary.
  auto* texture = gpu::gles2::Texture::CheckedCast(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->ShouldBindOrCopy() == gl::GLImage::BIND) {
      if (!image->BindTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to bind a gl image to texture.";
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to copy a gl image to texture.";
    }
  }
}

void SkiaOutputSurfaceImplOnGpu::OnSwapBuffers() {
  uint64_t swap_id = swap_id_++;
  gfx::Size pixel_size(sk_surface_->width(), sk_surface_->height());
  pending_swap_completed_params_.emplace_back(swap_id, pixel_size);
}

void SkiaOutputSurfaceImplOnGpu::CreateSkSurfaceForGL() {
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = gl_surface_->GetBackingFramebufferObject();
  framebuffer_info.fFormat = supports_alpha_ ? GL_RGBA8 : GL_RGB8_OES;
  GrBackendRenderTarget render_target(size_.width(), size_.height(), 0, 8,
                                      framebuffer_info);
  auto origin = capabilities_.flipped_output_surface
                    ? kTopLeft_GrSurfaceOrigin
                    : kBottomLeft_GrSurfaceOrigin;
  auto color_type =
      supports_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context(), render_target, origin, color_type,
      color_space_.ToSkColorSpace(), &surface_props);
  DCHECK(sk_surface_);
}

bool SkiaOutputSurfaceImplOnGpu::MakeCurrent(bool need_fbo0) {
  if (!is_using_vulkan()) {
    // Only make current with |gl_surface_|, if following operations will use
    // fbo0.
    if (!context_state_->MakeCurrent(need_fbo0 ? gl_surface_.get() : nullptr)) {
      LOG(ERROR) << "Failed to make current.";
      context_lost_callback_.Run();
      if (context_provider_)
        context_provider_->MarkContextLost();
      return false;
    }
    context_state_->set_need_context_state_reset(true);
  }
  return true;
}

void SkiaOutputSurfaceImplOnGpu::PullTextureUpdates(
    std::vector<gpu::SyncToken> sync_tokens) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (mailbox_manager_->UsesSync()) {
    for (auto& sync_token : sync_tokens)
      mailbox_manager_->PullTextureUpdates(sync_token);
  }
}

void SkiaOutputSurfaceImplOnGpu::ReleaseFenceSyncAndPushTextureUpdates(
    uint64_t sync_fence_release) {
  // TODO(https://crbug.com/900973): Remove it when MailboxManager is replaced
  // with SharedImage API.
  if (mailbox_manager_->UsesSync()) {
    // If MailboxManagerSync is used, we are sharing textures between threads.
    // In this case, sync point can only guarantee GL commands are issued in
    // correct order across threads and GL contexts. However GPU driver may
    // execute GL commands out of the issuing order across GL contexts. So we
    // have to use PushTextureUpdates() and PullTextureUpdates() to ensure the
    // correct GL commands executing order.
    // PushTextureUpdates(token) will insert a GL fence into the current GL
    // context, PullTextureUpdates(token) will wait the GL fence associated with
    // the give token on the current GL context.
    // Reconstruct sync_token from sync_fence_release.
    gpu::SyncToken sync_token(
        gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
        command_buffer_id(), sync_fence_release);
    mailbox_manager_->PushTextureUpdates(sync_token);
  }
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

}  // namespace viz
