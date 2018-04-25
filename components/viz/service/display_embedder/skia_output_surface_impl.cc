// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl.h"

#include "base/atomic_sequence_num.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
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
#include "gpu/ipc/service/image_transport_surface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

namespace {

base::AtomicSequenceNumber g_next_command_buffer_id;

}  // namespace

struct SkiaOutputSurfaceImpl::PromiseTextureInfo {
  PromiseTextureInfo(SkiaOutputSurfaceImpl* skia_renderer,
                     ResourceMetadata&& metadata)
      : skia_renderer(skia_renderer), resource_metadata(std::move(metadata)) {}
  ~PromiseTextureInfo() = default;

  SkiaOutputSurfaceImpl* const skia_renderer;
  ResourceMetadata resource_metadata;
};

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
    // We don't have a valid surface characterization, so we have to wait until
    // reshape is finished on Gpu thread.
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

bool SkiaOutputSurfaceImpl::SurfaceIsSuspendForRecycle() const {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);

  return false;
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

  auto texture_info =
      std::make_unique<PromiseTextureInfo>(this, std::move(metadata));
  const auto& data = texture_info->resource_metadata;
  auto image = recorder_->makePromiseTexture(
      data.backend_format, data.size.width(), data.size.height(),
      data.mip_mapped, data.origin, data.color_type, data.alpha_type,
      data.color_space, SkiaOutputSurfaceImpl::PromiseTextureFullfillStub,
      SkiaOutputSurfaceImpl::PromiseTextureReleaseStub,
      SkiaOutputSurfaceImpl::PromiseTextureDoneStub, texture_info.get());
  if (image)
    texture_info.release();
  return image;
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
  auto callback = base::BindOnce(&SkiaOutputSurfaceImpl::SwapBuffersOnGpuThread,
                                 base::Unretained(this), base::Passed(&frame),
                                 base::Passed(&ddl), sync_fence_release_);
  gpu_service_->scheduler()->ScheduleTask(gpu::Scheduler::Task(
      sequence_id, std::move(callback), std::move(resource_sync_tokens_)));
  return sync_token;
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

  // Destroy the surface with the context current, some surface destructors make
  // GL calls.
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
    uint64_t sync_fence_release) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
  DCHECK(ddl);
  DCHECK(sk_surface_);

  if (!gpu_service_->context_for_skia()->MakeCurrent(surface_.get())) {
    LOG(FATAL) << "Failed to make current.";
    // TODO(penghuang): Handle the failure.
  }
  sk_surface_->draw(ddl.get());
  gpu_service_->gr_context()->flush();
  surface_->SwapBuffers(
      base::BindRepeating([](const gfx::PresentationFeedback&) {}));
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release);
}

void SkiaOutputSurfaceImpl::RecreateRecorder() {
  DCHECK_CALLED_ON_VALID_THREAD(client_thread_checker_);
  DCHECK(characterization_.isValid());
  recorder_ =
      std::make_unique<SkDeferredDisplayListRecorder>(characterization_);
  // TODO(penghuang): remove the unnecessary getCanvas() call, when the recorder
  // crash is fixed in skia.
  recorder_->getCanvas();
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

// static
void SkiaOutputSurfaceImpl::PromiseTextureFullfillStub(
    void* texture_context,
    GrBackendTexture* backend_texture) {
  DCHECK(texture_context);
  auto* info = static_cast<PromiseTextureInfo*>(texture_context);
  info->skia_renderer->OnPromiseTextureFullfill(info->resource_metadata,
                                                backend_texture);
}

// static
void SkiaOutputSurfaceImpl::PromiseTextureReleaseStub(void* texture_context) {
  DCHECK(texture_context);
  auto* info = static_cast<PromiseTextureInfo*>(texture_context);
  info->skia_renderer->OnPromiseTextureRelease(info->resource_metadata);
}

// static
void SkiaOutputSurfaceImpl::PromiseTextureDoneStub(void* texture_context) {
  DCHECK(texture_context);
  std::unique_ptr<PromiseTextureInfo> info(
      static_cast<PromiseTextureInfo*>(texture_context));
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

  GrGLTextureInfo texture_info;
  texture_info.fTarget = texture_base->target();
  texture_info.fID = texture_base->service_id();
  // TODO(penghuang): Get the format correctly.
  texture_info.fFormat = GL_RGBA8;
  *backend_texture =
      GrBackendTexture(metadata.size.width(), metadata.size.height(),
                       metadata.mip_mapped, texture_info);
}

void SkiaOutputSurfaceImpl::OnPromiseTextureRelease(
    const ResourceMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(gpu_thread_checker_);
}

}  // namespace viz
