// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_impl_non_ddl.h"

#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"

namespace viz {

namespace {

scoped_refptr<gpu::SyncPointClientState> CreateSyncPointClientState(
    gpu::SyncPointManager* sync_point_manager,
    gpu::SequenceId sequence_id) {
  static uint64_t next_command_buffer_id = 0u;
  auto command_buffer_id =
      gpu::CommandBufferId::FromUnsafeValue(++next_command_buffer_id);
  return sync_point_manager->CreateSyncPointClientState(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
      command_buffer_id, sequence_id);
}

}  // namespace

SkiaOutputSurfaceImplNonDDL::SkiaOutputSurfaceImplNonDDL(
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::SharedContextState> shared_context_state,
    gpu::MailboxManager* mailbox_manager,
    gpu::SyncPointManager* sync_point_manager)
    : gl_surface_(std::move(gl_surface)),
      shared_context_state_(std::move(shared_context_state)),
      mailbox_manager_(mailbox_manager),
      sync_point_order_data_(sync_point_manager->CreateSyncPointOrderData()),
      sync_point_client_state_(
          CreateSyncPointClientState(sync_point_manager,
                                     sync_point_order_data_->sequence_id())) {}

SkiaOutputSurfaceImplNonDDL::~SkiaOutputSurfaceImplNonDDL() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaOutputSurfaceImplNonDDL::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceImplNonDDL::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void SkiaOutputSurfaceImplNonDDL::SetDrawRectangle(
    const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::Reshape(const gfx::Size& size,
                                          float device_scale_factor,
                                          const gfx::ColorSpace& color_space,
                                          bool has_alpha,
                                          bool use_stencil) {
  reshape_surface_size_ = size;
  reshape_device_scale_factor_ = device_scale_factor;
  reshape_color_space_ = color_space;
  reshape_has_alpha_ = has_alpha;
  reshape_use_stencil_ = use_stencil;
  backing_framebuffer_object_ = gl_surface_->GetBackingFramebufferObject();

  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = backing_framebuffer_object_;
  framebuffer_info.fFormat = GL_RGBA8;

  GrBackendRenderTarget render_target(size.width(), size.height(), 0, 8,
                                      framebuffer_info);

  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context(), render_target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, color_space.ToSkColorSpace(), &surface_props);
  DCHECK(sk_surface_);
}

void SkiaOutputSurfaceImplNonDDL::SwapBuffers(OutputSurfaceFrame frame) {
  NOTIMPLEMENTED();
}

uint32_t SkiaOutputSurfaceImplNonDDL::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return GL_RGB;
}

OverlayCandidateValidator*
SkiaOutputSurfaceImplNonDDL::GetOverlayCandidateValidator() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return nullptr;
}

bool SkiaOutputSurfaceImplNonDDL::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned SkiaOutputSurfaceImplNonDDL::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return 0;
}

gfx::BufferFormat SkiaOutputSurfaceImplNonDDL::GetOverlayBufferFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return gfx::BufferFormat::RGBX_8888;
}

bool SkiaOutputSurfaceImplNonDDL::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return false;
}

void SkiaOutputSurfaceImplNonDDL::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

#if BUILDFLAG(ENABLE_VULKAN)
gpu::VulkanSurface* SkiaOutputSurfaceImplNonDDL::GetVulkanSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

unsigned SkiaOutputSurfaceImplNonDDL::UpdateGpuFence() {
  return 0;
}

void SkiaOutputSurfaceImplNonDDL::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  NOTIMPLEMENTED();
}

SkCanvas* SkiaOutputSurfaceImplNonDDL::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(sk_surface_);
  DCHECK_EQ(current_render_pass_id_, 0u);
  DCHECK_EQ(order_num_, 0u);
  order_num_ = sync_point_order_data_->GenerateUnprocessedOrderNumber();
  sync_point_order_data_->BeginProcessingOrderNumber(order_num_);

  // If FBO is changed, we need call Reshape() to recreate |sk_surface_|.
  if (backing_framebuffer_object_ !=
      gl_surface_->GetBackingFramebufferObject()) {
    Reshape(reshape_surface_size_, reshape_device_scale_factor_,
            reshape_color_space_, reshape_has_alpha_, reshape_use_stencil_);
  }

  return sk_surface_->getCanvas();
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImage(
    ResourceMetadata metadata) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(metadata, &backend_texture)) {
    DLOG(ERROR) << "Failed to GetGrBackendTexture from mailbox.";
    return nullptr;
  }

  auto sk_color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, metadata.resource_format);
  return SkImage::MakeFromTexture(
      gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin, sk_color_type,
      metadata.alpha_type, metadata.color_space.ToSkColorSpace());
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImageFromYUV(
    std::vector<ResourceMetadata> metadatas,
    SkYUVColorSpace yuv_color_space,
    bool has_alpha) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK((has_alpha && (metadatas.size() == 3 || metadatas.size() == 4)) ||
         (!has_alpha && (metadatas.size() == 2 || metadatas.size() == 3)));

  bool is_i420 = has_alpha ? metadatas.size() == 4 : metadatas.size() == 3;

  GrBackendFormat formats[4];
  SkYUVAIndex indices[4] = {
      {-1, SkColorChannel::kR},
      {-1, SkColorChannel::kR},
      {-1, SkColorChannel::kR},
      {-1, SkColorChannel::kR},
  };
  GrBackendTexture yuva_textures[4] = {};
  const auto process_planar = [&](size_t i, ResourceFormat resource_format) {
    auto& metadata = metadatas[i];
    metadata.resource_format = resource_format;
    if (!GetGrBackendTexture(metadata, &yuva_textures[i]))
      DLOG(ERROR) << "Failed to GetGrBackendTexture from a mailbox.";
  };

  if (is_i420) {
    process_planar(0, RED_8);
    indices[SkYUVAIndex::kY_Index].fIndex = 0;
    indices[SkYUVAIndex::kY_Index].fChannel = SkColorChannel::kR;

    process_planar(1, RED_8);
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    process_planar(2, RED_8);
    indices[SkYUVAIndex::kV_Index].fIndex = 2;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kR;
    if (has_alpha) {
      process_planar(3, RED_8);
      indices[SkYUVAIndex::kA_Index].fIndex = 3;
      indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
    }
  } else {
    process_planar(0, RED_8);
    indices[SkYUVAIndex::kY_Index].fIndex = 0;
    indices[SkYUVAIndex::kY_Index].fChannel = SkColorChannel::kR;

    process_planar(1, RG_88);
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 1;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kG;
    if (has_alpha) {
      process_planar(2, RED_8);
      indices[SkYUVAIndex::kA_Index].fIndex = 2;
      indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
    }
  }

  return SkImage::MakeFromYUVATextures(
      gr_context(), yuv_color_space, yuva_textures, indices,
      SkISize::Make(yuva_textures[0].width(), yuva_textures[1].height()),
      kTopLeft_GrSurfaceOrigin);
}

gpu::SyncToken SkiaOutputSurfaceImplNonDDL::ReleasePromiseSkImages(
    std::vector<sk_sp<SkImage>> images) {
  if (images.empty())
    return gpu::SyncToken();
  DCHECK_EQ(order_num_, 0u);
  order_num_ = sync_point_order_data_->GenerateUnprocessedOrderNumber();
  sync_point_order_data_->BeginProcessingOrderNumber(order_num_);
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
      sync_point_client_state_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release_);
  DCHECK(mailbox_manager_->UsesSync());
  mailbox_manager_->PushTextureUpdates(sync_token);
  sync_point_order_data_->FinishProcessingOrderNumber(order_num_);
  order_num_ = 0u;
  return sync_token;
}

void SkiaOutputSurfaceImplNonDDL::SkiaSwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gl_surface_->SwapBuffers(
      base::DoNothing::Once<const gfx::PresentationFeedback&>());
}

SkCanvas* SkiaOutputSurfaceImplNonDDL::BeginPaintRenderPass(
    const RenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK_EQ(current_render_pass_id_, 0u);
  DCHECK_EQ(order_num_, 0u);
  order_num_ = sync_point_order_data_->GenerateUnprocessedOrderNumber();
  sync_point_order_data_->BeginProcessingOrderNumber(order_num_);
  current_render_pass_id_ = id;
  auto& sk_surface = offscreen_sk_surfaces_[id];

  if (!sk_surface) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    SkImageInfo image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    sk_surface =
        SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);
  }
  return sk_surface->getCanvas();
}

gpu::SyncToken SkiaOutputSurfaceImplNonDDL::SubmitPaint() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (current_render_pass_id_ == 0) {
    sk_surface_->flush();
  } else {
    offscreen_sk_surfaces_[current_render_pass_id_]->flush();
  }
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL,
      sync_point_client_state_->command_buffer_id(), ++sync_fence_release_);
  sync_token.SetVerifyFlush();
  sync_point_client_state_->ReleaseFenceSync(sync_fence_release_);
  DCHECK(mailbox_manager_->UsesSync());
  mailbox_manager_->PushTextureUpdates(sync_token);
  DCHECK_NE(order_num_, 0u);
  sync_point_order_data_->FinishProcessingOrderNumber(order_num_);
  order_num_ = 0u;
  current_render_pass_id_ = 0;
  return sync_token;
}

sk_sp<SkImage> SkiaOutputSurfaceImplNonDDL::MakePromiseSkImageFromRenderPass(
    const RenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = offscreen_sk_surfaces_.find(id);
  DCHECK(it != offscreen_sk_surfaces_.end());
  return it->second->makeImageSnapshot();
}

void SkiaOutputSurfaceImplNonDDL::RemoveRenderPassResource(
    std::vector<RenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  for (const auto& id : ids) {
    auto it = offscreen_sk_surfaces_.find(id);
    DCHECK(it != offscreen_sk_surfaces_.end());
    offscreen_sk_surfaces_.erase(it);
  }
}

void SkiaOutputSurfaceImplNonDDL::CopyOutput(
    RenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

void SkiaOutputSurfaceImplNonDDL::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceImplNonDDL::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SkiaOutputSurfaceImplNonDDL::GetGrBackendTexture(
    const ResourceMetadata& metadata,
    GrBackendTexture* backend_texture) {
  DCHECK(!metadata.mailbox_holder.mailbox.IsZero());

  base::WaitableEvent event;
  if (sync_point_client_state_->Wait(
          metadata.mailbox_holder.sync_token,
          base::BindOnce(&base::WaitableEvent::Signal,
                         base::Unretained(&event)))) {
    event.Wait();
    DCHECK(mailbox_manager_->UsesSync());
    mailbox_manager_->PullTextureUpdates(metadata.mailbox_holder.sync_token);
  }

  if (metadata.mailbox_holder.mailbox.IsSharedImage()) {
    // TODO(https://crbug.com/900973): support shared image.
  }

  auto* texture_base =
      mailbox_manager_->ConsumeTexture(metadata.mailbox_holder.mailbox);
  if (!texture_base) {
    DLOG(ERROR) << "Failed to make the SkImage";
    return false;
  }

  auto* gl_version_info =
      shared_context_state_->real_context()->GetVersionInfo();
  return gpu::GetGrBackendTexture(gl_version_info, texture_base->target(),
                                  metadata.size, texture_base->service_id(),
                                  metadata.resource_format, backend_texture);
}

void SkiaOutputSurfaceImplNonDDL::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnContextLost();
}

}  // namespace viz
