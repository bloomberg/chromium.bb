// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_metadata.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/viz_process_context_provider.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/gpu_preferences.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

namespace viz {
namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

}  // namespace

// Metadata for YUV promise SkImage.
class SkiaOutputSurfaceImpl::YUVResourceMetadata {
 public:
  YUVResourceMetadata(std::vector<ResourceMetadata> metadatas,
                      SkYUVColorSpace yuv_color_space)
      : metadatas_(std::move(metadatas)), yuv_color_space_(yuv_color_space) {
    DCHECK(metadatas_.size() == 2 || metadatas_.size() == 3);
  }
  YUVResourceMetadata(YUVResourceMetadata&& other) = default;
  ~YUVResourceMetadata() = default;
  YUVResourceMetadata& operator=(YUVResourceMetadata&& other) = default;

  const std::vector<ResourceMetadata>& metadatas() const { return metadatas_; }
  SkYUVColorSpace yuv_color_space() const { return yuv_color_space_; }
  const sk_sp<SkImage> image() const { return image_; }
  void set_image(sk_sp<SkImage> image) { image_ = image; }
  const gfx::Size size() const { return metadatas_[0].size; }

 private:
  // Resource metadatas for YUV planes.
  std::vector<ResourceMetadata> metadatas_;

  SkYUVColorSpace yuv_color_space_;

  // The image copied from YUV textures, it is for fullfilling the promise
  // image.
  // TODO(penghuang): Remove it when Skia supports drawing YUV textures
  // directly.
  sk_sp<SkImage> image_;

  DISALLOW_COPY_AND_ASSIGN(YUVResourceMetadata);
};

// A helper class for fullfilling promise image on the GPU thread.
template <class FullfillContextType>
class SkiaOutputSurfaceImpl::PromiseTextureHelper {
 public:
  using HelperType = PromiseTextureHelper<FullfillContextType>;

  PromiseTextureHelper(SkiaOutputSurfaceImpl* impl, FullfillContextType context)
      : impl_(impl), context_(std::move(context)) {}
  ~PromiseTextureHelper() = default;

  static sk_sp<SkImage> MakePromiseSkImage(
      SkiaOutputSurfaceImpl* impl,
      SkDeferredDisplayListRecorder* recorder,
      const GrBackendFormat& backend_format,
      gfx::Size size,
      GrMipMapped mip_mapped,
      GrSurfaceOrigin origin,
      SkColorType color_type,
      SkAlphaType alpha_type,
      sk_sp<SkColorSpace> color_space,
      FullfillContextType context) {
    DCHECK_CALLED_ON_VALID_THREAD(impl->client_thread_checker_);
    auto helper = std::make_unique<HelperType>(impl, std::move(context));
    auto image = recorder->makePromiseTexture(
        backend_format, size.width(), size.height(), mip_mapped, origin,
        color_type, alpha_type, color_space, HelperType::Fullfill,
        HelperType::Release, HelperType::Done, helper.get());
    if (image) {
      helper->Init(impl);
      helper.release();
    }
    return image;
  }

 private:
  void Init(SkiaOutputSurfaceImpl* impl);

  static void Fullfill(void* texture_context,
                       GrBackendTexture* backend_texture) {
    DCHECK(texture_context);
    auto* helper = static_cast<HelperType*>(texture_context);
    DCHECK_CALLED_ON_VALID_THREAD(helper->impl_->gpu_thread_checker_);
    helper->impl_->OnPromiseTextureFullfill(helper->context_, backend_texture);
  }

  static void Release(void* texture_context) { DCHECK(texture_context); }

  static void Done(void* texture_context) {
    DCHECK(texture_context);
    std::unique_ptr<HelperType> helper(
        static_cast<HelperType*>(texture_context));
  }

  SkiaOutputSurfaceImpl* const impl_;

  // The data for calling the fullfill methods in SkiaOutputSurfaceImpl.
  FullfillContextType context_;

  DISALLOW_COPY_AND_ASSIGN(PromiseTextureHelper);
};

template <class T>
void SkiaOutputSurfaceImpl::PromiseTextureHelper<T>::Init(
    SkiaOutputSurfaceImpl* impl) {}

// For YUVResourceMetadata, we need to record the |context_| pointer in
// |impl->yuv_resource_metadatas_|, because we have to create SkImage from YUV
// textures before drawing the ddl to a SKSurface.
// TODO(penghuang): Remove this hack when Skia supports drawing YUV textures
// directly.
template <>
void SkiaOutputSurfaceImpl::PromiseTextureHelper<
    SkiaOutputSurfaceImpl::YUVResourceMetadata>::Init(SkiaOutputSurfaceImpl*
                                                          impl) {
  impl->yuv_resource_metadatas_.push_back(&context_);
}

SkiaOutputSurfaceImpl::SkiaOutputSurfaceImpl(
    GpuServiceImpl* gpu_service,
    gpu::SurfaceHandle surface_handle,
    scoped_refptr<VizProcessContextProvider> context_provider,
    SyntheticBeginFrameSource* synthetic_begin_frame_source)
    : SkiaOutputSurface(context_provider),
      command_buffer_id_(gpu::CommandBufferId::FromUnsafeValue(
          g_next_command_buffer_id.GetNext() + 1)),
      gpu_service_(gpu_service),
      surface_handle_(surface_handle),
      synthetic_begin_frame_source_(synthetic_begin_frame_source),
      gpu_thread_weak_ptr_factory_(this),
      client_thread_weak_ptr_factory_(this) {
  DETACH_FROM_THREAD(client_thread_checker_);
  DETACH_FROM_THREAD(gpu_thread_checker_);
}

SkiaOutputSurfaceImpl::~SkiaOutputSurfaceImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  recorder_ = nullptr;
  // TODO(penghuang): avoid blocking compositor thread.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::DestroyOnGpuThread,
                                 base::Unretained(this), &event);
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::vector<gpu::SyncToken>()));
  event.Wait();
}

void SkiaOutputSurfaceImpl::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(client);
  DCHECK(!client_);

  client_ = client;
  client_thread_weak_ptr_ = client_thread_weak_ptr_factory_.GetWeakPtr();
  client_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();

  // TODO(penghuang): avoid blocking compositor thread.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::InitializeOnGpuThread,
                                 base::Unretained(this), &event);
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::vector<gpu::SyncToken>()));
  event.Wait();
}

void SkiaOutputSurfaceImpl::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImpl::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImpl::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void SkiaOutputSurfaceImpl::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImpl::Reshape(const gfx::Size& size,
                                    float device_scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha,
                                    bool use_stencil) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  recorder_ = nullptr;
  SkSurfaceCharacterization* characterization = nullptr;
  std::unique_ptr<base::WaitableEvent> event;
  if (characterization_.isValid()) {
    characterization_ =
        characterization_.createResized(size.width(), size.height());
  } else {
    characterization = &characterization_;
    // TODO(penghuang): avoid blocking compositor thread.
    // We don't have a valid surface characterization, so we have to wait
    // until reshape is finished on Gpu thread.
    event = std::make_unique<base::WaitableEvent>(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
  }

  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::ReshapeOnGpuThread,
                                 base::Unretained(this), size,
                                 device_scale_factor, color_space, has_alpha,
                                 use_stencil, characterization, event.get());
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::vector<gpu::SyncToken>()));

  if (event)
    event->Wait();
  RecreateRecorder();
}

void SkiaOutputSurfaceImpl::SwapBuffers(OutputSurfaceFrame frame) {
  NOTREACHED();
}

uint32_t SkiaOutputSurfaceImpl::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  return GL_RGB;
}

OverlayCandidateValidator* SkiaOutputSurfaceImpl::GetOverlayCandidateValidator()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  return nullptr;
}

bool SkiaOutputSurfaceImpl::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned SkiaOutputSurfaceImpl::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  return 0;
}

gfx::BufferFormat SkiaOutputSurfaceImpl::GetOverlayBufferFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  return gfx::BufferFormat::RGBX_8888;
}

bool SkiaOutputSurfaceImpl::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  return false;
}

void SkiaOutputSurfaceImpl::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* SkiaOutputSurfaceImpl::GetVulkanSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  return nullptr;
}
#endif

SkCanvas* SkiaOutputSurfaceImpl::GetSkCanvasForCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(recorder_);
  DCHECK_EQ(current_render_pass_id_, 0u);

  return recorder_->getCanvas();
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImage(
    ResourceMetadata metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(recorder_);

  // Convert internal format from GLES2 to platform GL.
  const auto* version_info = gpu_service_->context_for_skia()->GetVersionInfo();
  metadata.backend_format = GrBackendFormat::MakeGL(
      gl::GetInternalFormat(version_info,
                            *metadata.backend_format.getGLFormat()),
      *metadata.backend_format.getGLTarget());

  DCHECK(!metadata.mailbox.IsZero());
  resource_sync_tokens_.push_back(metadata.sync_token);

  return PromiseTextureHelper<ResourceMetadata>::MakePromiseSkImage(
      this, recorder_.get(), metadata.backend_format, metadata.size,
      metadata.mip_mapped, metadata.origin, metadata.color_type,
      metadata.alpha_type, metadata.color_space, std::move(metadata));
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromYUV(
    std::vector<ResourceMetadata> metadatas,
    SkYUVColorSpace yuv_color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(recorder_);

  DCHECK(metadatas.size() == 2 || metadatas.size() == 3);

  // TODO(penghuang): Create SkImage from YUV textures directly when it is
  // supported by Skia.
  YUVResourceMetadata yuv_metadata(std::move(metadatas), yuv_color_space);

  // Convert internal format from GLES2 to platform GL.
  const auto* version_info = gpu_service_->context_for_skia()->GetVersionInfo();
  auto backend_format = GrBackendFormat::MakeGL(
      gl::GetInternalFormat(version_info, GL_BGRA8_EXT), GL_TEXTURE_2D);

  return PromiseTextureHelper<YUVResourceMetadata>::MakePromiseSkImage(
      this, recorder_.get(), backend_format, yuv_metadata.size(),
      GrMipMapped::kNo, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, nullptr /* color_space */, std::move(yuv_metadata));
}

gpu::SyncToken SkiaOutputSurfaceImpl::SkiaSwapBuffers(
    OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(recorder_);

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::VIZ_OUTPUT_SURFACE,
                            command_buffer_id_, ++sync_fence_release_);
  sync_token.SetVerifyFlush();

  auto ddl = recorder_->detach();
  DCHECK(ddl);
  RecreateRecorder();
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback =
      base::BindOnce(&SkiaOutputSurfaceImpl::SwapBuffersOnGpuThread,
                     base::Unretained(this), std::move(frame), std::move(ddl),
                     std::move(yuv_resource_metadatas_), sync_fence_release_);
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::move(resource_sync_tokens_)));
  return sync_token;
}

SkCanvas* SkiaOutputSurfaceImpl::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(gpu_service_->gr_context());
  DCHECK(!current_render_pass_id_);
  DCHECK(!offscreen_surface_recorder_);
  DCHECK(resource_sync_tokens_.empty());

  current_render_pass_id_ = id;

  auto gr_context_thread_safe = gpu_service_->gr_context()->threadSafeProxy();
  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /*gpu_compositing */, format);
  SkImageInfo image_info =
      SkImageInfo::Make(surface_size.width(), surface_size.height(), color_type,
                        kPremul_SkAlphaType, nullptr /* color_space */);

  // TODO(penghuang): Figure out how to choose the right size.
  constexpr size_t kCacheMaxResourceBytes = 90 * 1024 * 1024;
  const auto* version_info = gpu_service_->context_for_skia()->GetVersionInfo();
  unsigned int texture_storage_format = TextureStorageFormat(format);
  auto backend_format = GrBackendFormat::MakeGL(
      gl::GetInternalFormat(version_info, texture_storage_format),
      GL_TEXTURE_2D);
  auto characterization = gr_context_thread_safe->createCharacterization(
      kCacheMaxResourceBytes, image_info, backend_format, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, surface_props, mipmap);
  DCHECK(characterization.isValid());
  offscreen_surface_recorder_ =
      std::make_unique<SkDeferredDisplayListRecorder>(characterization);
  return offscreen_surface_recorder_->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImpl::FinishPaintRenderPass() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(gpu_service_->gr_context());
  DCHECK(current_render_pass_id_);
  DCHECK(offscreen_surface_recorder_);

  gpu::SyncToken sync_token(gpu::CommandBufferNamespace::VIZ_OUTPUT_SURFACE,
                            command_buffer_id_, ++sync_fence_release_);

  auto ddl = offscreen_surface_recorder_->detach();
  offscreen_surface_recorder_ = nullptr;
  DCHECK(ddl);

  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImpl::FinishPaintRenderPassOnGpuThread,
      base::Unretained(this), current_render_pass_id_, std::move(ddl),
      std::move(yuv_resource_metadatas_), sync_fence_release_);
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::move(resource_sync_tokens_)));
  current_render_pass_id_ = 0;
  sync_token.SetVerifyFlush();
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImpl::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(recorder_);

  // Convert internal format from GLES2 to platform GL.
  const auto* version_info = gpu_service_->context_for_skia()->GetVersionInfo();
  unsigned int texture_storage_format = TextureStorageFormat(format);
  auto backend_format = GrBackendFormat::MakeGL(
      gl::GetInternalFormat(version_info, texture_storage_format),
      GL_TEXTURE_2D);
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /*gpu_compositing */, format);
  return PromiseTextureHelper<RenderPassId>::MakePromiseSkImage(
      this, recorder_.get(), backend_format, size,
      mipmap ? GrMipMapped::kYes : GrMipMapped::kNo, kTopLeft_GrSurfaceOrigin,
      color_type, kPremul_SkAlphaType, nullptr /* color_space */, id);
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK(!ids.empty());
  auto sequence_id = gpu_service_->skia_output_surface_sequence_id();
  auto callback = base::BindOnce(
      &SkiaOutputSurfaceImpl::RemoveRenderPassResourceOnGpuThread,
      base::Unretained(this), std::move(ids));
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::vector<gpu::SyncToken>()));
}

void SkiaOutputSurfaceImpl::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  client_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SkiaOutputSurfaceImpl::DidSwapBuffersCompleteOnClientThread,
          client_thread_weak_ptr_, params));
}

const gpu::gles2::FeatureInfo* SkiaOutputSurfaceImpl::GetFeatureInfo() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

const gpu::GpuPreferences& SkiaOutputSurfaceImpl::GetGpuPreferences() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  NOTIMPLEMENTED();
  return gpu_preferences_;
}

void SkiaOutputSurfaceImpl::SetSnapshotRequestedCallback(
    const base::Closure& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImpl::UpdateVSyncParameters(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  client_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SkiaOutputSurfaceImpl::UpdateVSyncParametersOnClientThread,
          client_thread_weak_ptr_, timebase, interval));
}

void SkiaOutputSurfaceImpl::BufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  client_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SkiaOutputSurfaceImpl::BufferPresentedOnClientThread,
                     client_thread_weak_ptr_, swap_id, feedback));
}

void SkiaOutputSurfaceImpl::AddFilter(IPC::MessageFilter* message_filter) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  NOTIMPLEMENTED();
}

int32_t SkiaOutputSurfaceImpl::GetRouteID() const {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  NOTIMPLEMENTED();
  return 0;
}

void SkiaOutputSurfaceImpl::InitializeOnGpuThread(base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  base::ScopedClosureRunner scoped_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));

  sync_point_client_state_ =
      gpu_service_->sync_point_manager()->CreateSyncPointClientState(
          gpu::CommandBufferNamespace::VIZ_OUTPUT_SURFACE, command_buffer_id_,
          gpu_service_->skia_output_surface_sequence_id());

  surface_ = gpu::ImageTransportSurface::CreateNativeSurface(
      gpu_thread_weak_ptr_factory_.GetWeakPtr(), surface_handle_,
      gl::GLSurfaceFormat());
  DCHECK(surface_);

  if (!gpu_service_->CreateGrContextIfNecessary(surface_.get())) {
    LOG(FATAL) << "Failed to create GrContext";
    // TODO(penghuang): handle the failure.
  }

  DCHECK(gpu_service_->context_for_skia());
  DCHECK(gpu_service_->gr_context());

  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle the failure.
  }

  capabilities_.flipped_output_surface = surface_->FlipsVertically();

  // Get stencil bits from the default frame buffer.
  auto* current_gl = gpu_service_->context_for_skia()->GetCurrentGL();
  const auto* version = current_gl->Version;
  auto* api = current_gl->Api;
  GLint stencil_bits = 0;
  if (version->is_desktop_core_profile) {
    api->glGetFramebufferAttachmentParameterivEXTFn(
        GL_FRAMEBUFFER, GL_STENCIL, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE,
        &stencil_bits);
  } else {
    api->glGetIntegervFn(GL_STENCIL_BITS, &stencil_bits);
  }

  capabilities_.supports_stencil = stencil_bits > 0;
}

void SkiaOutputSurfaceImpl::DestroyOnGpuThread(base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  base::ScopedClosureRunner scoped_runner(
      base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  gpu_thread_weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy the surface with the context current, some surface destructors
  // make GL calls.
  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle the failure.
  }
  surface_ = nullptr;
  sync_point_client_state_ = nullptr;
}

void SkiaOutputSurfaceImpl::ReshapeOnGpuThread(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil,
    SkSurfaceCharacterization* characterization,
    base::WaitableEvent* event) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  std::unique_ptr<base::ScopedClosureRunner> scoped_runner;
  if (event) {
    scoped_runner = std::make_unique<base::ScopedClosureRunner>(
        base::BindOnce(&base::WaitableEvent::Signal, base::Unretained(event)));
  }

  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle the failure.
  }
  gl::GLSurface::ColorSpace surface_color_space =
      color_space == gfx::ColorSpace::CreateSCRGBLinear()
          ? gl::GLSurface::ColorSpace::SCRGB_LINEAR
          : gl::GLSurface::ColorSpace::UNSPECIFIED;
  if (!surface_->Resize(size, device_scale_factor, surface_color_space,
                        has_alpha)) {
    LOG(FATAL) << "Failed to resize.";
    // TODO(penghuang): Handle the failure.
  }
  DCHECK(gpu_service_->context_for_skia()->IsCurrent(surface_.get()));
  DCHECK(gpu_service_->gr_context());

  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = 0;
  const auto* version_info = gpu_service_->context_for_skia()->GetVersionInfo();
  framebuffer_info.fFormat = version_info->is_es ? GL_BGRA8_EXT : GL_RGBA8;

  GrBackendRenderTarget render_target(size.width(), size.height(), 0, 8,
                                      framebuffer_info);

  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gpu_service_->gr_context(), render_target, kBottomLeft_GrSurfaceOrigin,
      kBGRA_8888_SkColorType, nullptr, &surface_props);
  DCHECK(sk_surface_);

  if (characterization) {
    sk_surface_->characterize(characterization);
    DCHECK(characterization->isValid());
  }
}

void SkiaOutputSurfaceImpl::SwapBuffersOnGpuThread(
    OutputSurfaceFrame frame,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<YUVResourceMetadata*> yuv_resource_metadatas,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  DCHECK(ddl);
  DCHECK(sk_surface_);

  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle the failure.
  }

  PreprocessYUVResources(std::move(yuv_resource_metadatas));

  sk_surface_->draw(ddl.get());
  gpu_service_->gr_context()->flush();
  surface_->SwapBuffers(
      base::BindRepeating([](const gfx::PresentationFeedback&) {}));
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

void SkiaOutputSurfaceImpl::FinishPaintRenderPassOnGpuThread(
    RenderPassId id,
    std::unique_ptr<SkDeferredDisplayList> ddl,
    std::vector<YUVResourceMetadata*> yuv_resource_metadatas,
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  DCHECK(ddl);

  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle resize failure.
  }

  PreprocessYUVResources(std::move(yuv_resource_metadatas));

  auto& surface = offscreen_surfaces_[id];
  SkSurfaceCharacterization characterization;
  // TODO(penghuang): Using characterization != ddl->characterization(), when
  // the SkSurfaceCharacterization::operator!= is implemented in Skia.
  if (!surface || !surface->characterize(&characterization) ||
      characterization != ddl->characterization()) {
    surface = SkSurface::MakeRenderTarget(
        gpu_service_->gr_context(), ddl->characterization(), SkBudgeted::kNo);
    DCHECK(surface);
  }
  surface->draw(ddl.get());
  surface->flush();
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

void SkiaOutputSurfaceImpl::RemoveRenderPassResourceOnGpuThread(
    std::vector<RenderPassId> ids) {
  DCHECK(!ids.empty());
  for (const auto& id : ids) {
    auto it = offscreen_surfaces_.find(id);
    DCHECK(it != offscreen_surfaces_.end());
    offscreen_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImpl::RecreateRecorder() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(characterization_.isValid());
  recorder_ =
      std::make_unique<SkDeferredDisplayListRecorder>(characterization_);
  // TODO(penghuang): remove the unnecessary getCanvas() call, when the
  // recorder crash is fixed in skia.
  recorder_->getCanvas();
}

void SkiaOutputSurfaceImpl::PreprocessYUVResources(
    std::vector<YUVResourceMetadata*> yuv_resource_metadatas) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  // Create SkImage for fullfilling YUV promise image, before drawing the ddl.
  // TODO(penghuang): Remove the extra step when Skia supports drawing YUV
  // textures directly.
  auto* mailbox_manager = gpu_service_->mailbox_manager();
  for (auto* yuv_metadata : yuv_resource_metadatas) {
    const auto& metadatas = yuv_metadata->metadatas();
    DCHECK(metadatas.size() == 2 || metadatas.size() == 3);
    GrBackendTexture backend_textures[3];
    size_t i = 0;
    for (const auto& metadata : metadatas) {
      auto* texture_base = mailbox_manager->ConsumeTexture(metadata.mailbox);
      if (!texture_base)
        break;
      BindOrCopyTextureIfNecessary(texture_base);
      GrGLTextureInfo texture_info;
      texture_info.fTarget = texture_base->target();
      texture_info.fID = texture_base->service_id();
      texture_info.fFormat = *metadata.backend_format.getGLFormat();
      backend_textures[i++] =
          GrBackendTexture(metadata.size.width(), metadata.size.height(),
                           GrMipMapped::kNo, texture_info);
    }

    if (i != metadatas.size())
      continue;

    sk_sp<SkImage> image;
    if (metadatas.size() == 2) {
      image = SkImage::MakeFromNV12TexturesCopy(
          gpu_service_->gr_context(), yuv_metadata->yuv_color_space(),
          backend_textures, kTopLeft_GrSurfaceOrigin,
          nullptr /* image_color_space */);
      DCHECK(image);
    } else {
      image = SkImage::MakeFromYUVTexturesCopy(
          gpu_service_->gr_context(), yuv_metadata->yuv_color_space(),
          backend_textures, kTopLeft_GrSurfaceOrigin,
          nullptr /* image_color_space */);
      DCHECK(image);
    }
    yuv_metadata->set_image(std::move(image));
  }
}

void SkiaOutputSurfaceImpl::BindOrCopyTextureIfNecessary(
    gpu::TextureBase* texture_base) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  if (gpu_service_->gpu_preferences().use_passthrough_cmd_decoder)
    return;

  // If a texture created with non-passthrough command buffer and bind with
  // an image, the Chrome will defer copying the image to the texture until
  // the texture is used. It is for implementing low latency drawing and
  // avoiding unnecessary texture copy. So we need check the texture image
  // state, and bind or copy the image to the texture if necessary.
  auto* texture = static_cast<gpu::gles2::Texture*>(texture_base);
  gpu::gles2::Texture::ImageState image_state;
  auto* image = texture->GetLevelImage(GL_TEXTURE_2D, 0, &image_state);
  if (image && image_state == gpu::gles2::Texture::UNBOUND) {
    glBindTexture(texture_base->target(), texture_base->service_id());
    if (image->BindTexImage(texture_base->target())) {
    } else {
      texture->SetLevelImageState(texture_base->target(), 0,
                                  gpu::gles2::Texture::COPIED);
      if (!image->CopyTexImage(texture_base->target()))
        LOG(ERROR) << "Failed to copy a gl image to texture.";
    }
  }
}

void SkiaOutputSurfaceImpl::DidSwapBuffersCompleteOnClientThread(
    gpu::SwapBuffersCompleteParams params) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(client_);

  if (!params.texture_in_use_responses.empty())
    client_->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
  if (!params.ca_layer_params.is_empty)
    client_->DidReceiveCALayerParams(params.ca_layer_params);
  client_->DidReceiveSwapBuffersAck(params.swap_response.swap_id);
}

void SkiaOutputSurfaceImpl::UpdateVSyncParametersOnClientThread(
    base::TimeTicks timebase,
    base::TimeDelta interval) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  if (synthetic_begin_frame_source_) {
    // TODO(brianderson): We should not be receiving 0 intervals.
    synthetic_begin_frame_source_->OnUpdateVSyncParameters(
        timebase,
        interval.is_zero() ? BeginFrameArgs::DefaultInterval() : interval);
  }
}

void SkiaOutputSurfaceImpl::BufferPresentedOnClientThread(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(client_);

  client_->DidReceivePresentationFeedback(swap_id, feedback);
}

void SkiaOutputSurfaceImpl::OnPromiseTextureFullfill(
    const ResourceMetadata& metadata,
    GrBackendTexture* backend_texture) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  auto* mailbox_manager = gpu_service_->mailbox_manager();
  auto* texture_base = mailbox_manager->ConsumeTexture(metadata.mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to full fill the promise texture.";
    return;
  }
  BindOrCopyTextureIfNecessary(texture_base);
  GrGLTextureInfo texture_info;
  texture_info.fTarget = texture_base->target();
  texture_info.fID = texture_base->service_id();
  texture_info.fFormat = *metadata.backend_format.getGLFormat();
  *backend_texture =
      GrBackendTexture(metadata.size.width(), metadata.size.height(),
                       metadata.mip_mapped, texture_info);
}

void SkiaOutputSurfaceImpl::OnPromiseTextureFullfill(
    const YUVResourceMetadata& yuv_metadata,
    GrBackendTexture* backend_texture) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);

  if (yuv_metadata.image())
    *backend_texture = yuv_metadata.image()->getBackendTexture(true);
  DLOG_IF(ERROR, !backend_texture->isValid())
      << "Failed to full fill the promise texture from yuv resources.";
}

void SkiaOutputSurfaceImpl::OnPromiseTextureFullfill(
    const RenderPassId id,
    GrBackendTexture* backend_texture) {
  auto it = offscreen_surfaces_.find(id);
  DCHECK(it != offscreen_surfaces_.end());
  sk_sp<SkSurface>& surface = it->second;
  *backend_texture =
      surface->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess);
  DLOG_IF(ERROR, !backend_texture->isValid())
      << "Failed to full fill the promise texture created from RenderPassId:"
      << id;
}

}  // namespace viz
