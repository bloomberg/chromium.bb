// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bit_cast.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/resources/layer_tree_resource_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/half_float_maker.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

namespace {

VideoFrameExternalResources::ResourceType ExternalResourceTypeForHardwarePlanes(
    media::VideoPixelFormat format,
    GLuint target,
    int num_textures,
    gfx::BufferFormat* buffer_format,
    bool use_stream_video_draw_quad) {
  *buffer_format = gfx::BufferFormat::RGBA_8888;
  switch (format) {
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_RGB32:
    case media::PIXEL_FORMAT_UYVY:
      switch (target) {
        case GL_TEXTURE_EXTERNAL_OES:
          if (use_stream_video_draw_quad)
            return VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE;
          FALLTHROUGH;
        case GL_TEXTURE_2D:
          return (format == media::PIXEL_FORMAT_XRGB)
                     ? VideoFrameExternalResources::RGB_RESOURCE
                     : VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE;
        case GL_TEXTURE_RECTANGLE_ARB:
          return VideoFrameExternalResources::RGB_RESOURCE;
        default:
          NOTREACHED();
          break;
      }
      break;
    case media::PIXEL_FORMAT_I420:
      return VideoFrameExternalResources::YUV_RESOURCE;
    case media::PIXEL_FORMAT_NV12:
      DCHECK(target == GL_TEXTURE_EXTERNAL_OES || target == GL_TEXTURE_2D ||
             target == GL_TEXTURE_RECTANGLE_ARB)
          << "Unsupported texture target " << std::hex << std::showbase
          << target;
      // Single plane textures can be sampled as RGB.
      if (num_textures > 1)
        return VideoFrameExternalResources::YUV_RESOURCE;

      *buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
      return VideoFrameExternalResources::RGB_RESOURCE;
    case media::PIXEL_FORMAT_YV12:
    case media::PIXEL_FORMAT_I422:
    case media::PIXEL_FORMAT_I444:
    case media::PIXEL_FORMAT_I420A:
    case media::PIXEL_FORMAT_NV21:
    case media::PIXEL_FORMAT_YUY2:
    case media::PIXEL_FORMAT_RGB24:
    case media::PIXEL_FORMAT_MJPEG:
    case media::PIXEL_FORMAT_MT21:
    case media::PIXEL_FORMAT_YUV420P9:
    case media::PIXEL_FORMAT_YUV422P9:
    case media::PIXEL_FORMAT_YUV444P9:
    case media::PIXEL_FORMAT_YUV420P10:
    case media::PIXEL_FORMAT_YUV422P10:
    case media::PIXEL_FORMAT_YUV444P10:
    case media::PIXEL_FORMAT_YUV420P12:
    case media::PIXEL_FORMAT_YUV422P12:
    case media::PIXEL_FORMAT_YUV444P12:
    case media::PIXEL_FORMAT_Y16:
    case media::PIXEL_FORMAT_UNKNOWN:
      break;
  }
  return VideoFrameExternalResources::NONE;
}

class SyncTokenClientImpl : public media::VideoFrame::SyncTokenClient {
 public:
  SyncTokenClientImpl(gpu::gles2::GLES2Interface* gl, gpu::SyncToken sync_token)
      : gl_(gl), sync_token_(sync_token) {}
  ~SyncTokenClientImpl() override = default;

  void GenerateSyncToken(gpu::SyncToken* sync_token) override {
    if (sync_token_.HasData()) {
      *sync_token = sync_token_;
    } else {
      gl_->GenSyncTokenCHROMIUM(sync_token->GetData());
    }
  }

  void WaitSyncToken(const gpu::SyncToken& sync_token) override {
    if (sync_token.HasData()) {
      gl_->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
      if (sync_token_.HasData() && sync_token_ != sync_token) {
        gl_->WaitSyncTokenCHROMIUM(sync_token_.GetConstData());
        sync_token_.Clear();
      }
    }
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
  gpu::SyncToken sync_token_;
  DISALLOW_COPY_AND_ASSIGN(SyncTokenClientImpl);
};

// Sync tokens passed downstream to the compositor can be unverified.
void GenerateCompositorSyncToken(gpu::gles2::GLES2Interface* gl,
                                 gpu::SyncToken* sync_token) {
  gl->GenUnverifiedSyncTokenCHROMIUM(sync_token->GetData());
}

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
gfx::Size SoftwarePlaneDimension(media::VideoFrame* input_frame,
                                 bool software_compositor,
                                 size_t plane_index) {
  gfx::Size coded_size = input_frame->coded_size();
  if (software_compositor)
    return coded_size;

  int plane_width = media::VideoFrame::Columns(
      plane_index, input_frame->format(), coded_size.width());
  int plane_height = media::VideoFrame::Rows(plane_index, input_frame->format(),
                                             coded_size.height());
  return gfx::Size(plane_width, plane_height);
}

}  // namespace

VideoFrameExternalResources::VideoFrameExternalResources() = default;
VideoFrameExternalResources::~VideoFrameExternalResources() = default;

VideoFrameExternalResources::VideoFrameExternalResources(
    VideoFrameExternalResources&& other) = default;
VideoFrameExternalResources& VideoFrameExternalResources::operator=(
    VideoFrameExternalResources&& other) = default;

VideoResourceUpdater::PlaneResource::PlaneResource(
    viz::ResourceId resource_id,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    gpu::Mailbox mailbox)
    : resource_id_(resource_id),
      resource_size_(resource_size),
      resource_format_(resource_format),
      mailbox_(mailbox) {}

bool VideoResourceUpdater::PlaneResource::Matches(int unique_frame_id,
                                                  size_t plane_index) {
  return has_unique_frame_id_and_plane_index_ &&
         unique_frame_id_ == unique_frame_id && plane_index_ == plane_index;
}

void VideoResourceUpdater::PlaneResource::SetUniqueId(int unique_frame_id,
                                                      size_t plane_index) {
  DCHECK_EQ(ref_count_, 1);
  plane_index_ = plane_index;
  unique_frame_id_ = unique_frame_id;
  has_unique_frame_id_and_plane_index_ = true;
}

VideoResourceUpdater::VideoResourceUpdater(
    viz::ContextProvider* context_provider,
    LayerTreeResourceProvider* resource_provider,
    bool use_stream_video_draw_quad)
    : context_provider_(context_provider),
      resource_provider_(resource_provider),
      use_stream_video_draw_quad_(use_stream_video_draw_quad),
      weak_ptr_factory_(this) {}

VideoResourceUpdater::~VideoResourceUpdater() {
  for (const PlaneResource& plane_resource : all_resources_)
    resource_provider_->DeleteResource(plane_resource.resource_id());
}

void VideoResourceUpdater::ObtainFrameResources(
    scoped_refptr<media::VideoFrame> video_frame) {
  VideoFrameExternalResources external_resources =
      CreateExternalResourcesFromVideoFrame(video_frame);
  frame_resource_type_ = external_resources.type;

  if (external_resources.type ==
      VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    DCHECK_GT(external_resources.software_resource, viz::kInvalidResourceId);
    software_resource_ = external_resources.software_resource;
    software_release_callback_ =
        std::move(external_resources.software_release_callback);
  } else {
    frame_resource_offset_ = external_resources.offset;
    frame_resource_multiplier_ = external_resources.multiplier;
    frame_bits_per_channel_ = external_resources.bits_per_channel;

    DCHECK_EQ(external_resources.resources.size(),
              external_resources.release_callbacks.size());
    for (size_t i = 0; i < external_resources.resources.size(); ++i) {
      viz::ResourceId resource_id = resource_provider_->ImportResource(
          external_resources.resources[i],
          viz::SingleReleaseCallback::Create(
              std::move(external_resources.release_callbacks[i])));
      frame_resources_.push_back(
          {resource_id, external_resources.resources[i].size});
    }
  }
  TRACE_EVENT_INSTANT1("media", "VideoResourceUpdater::ObtainFrameResources",
                       TRACE_EVENT_SCOPE_THREAD, "Timestamp",
                       video_frame->timestamp().InMicroseconds());
}

void VideoResourceUpdater::ReleaseFrameResources() {
  if (frame_resource_type_ == VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    DCHECK_GT(software_resource_, viz::kInvalidResourceId);
    std::move(software_release_callback_).Run(gpu::SyncToken(), false);
    software_resource_ = viz::kInvalidResourceId;
  } else {
    for (auto& frame_resource : frame_resources_)
      resource_provider_->RemoveImportedResource(frame_resource.id);
    frame_resources_.clear();
  }
}

void VideoResourceUpdater::AppendQuads(viz::RenderPass* render_pass,
                                       scoped_refptr<media::VideoFrame> frame,
                                       gfx::Transform transform,
                                       gfx::Size rotated_size,
                                       gfx::Rect visible_layer_rect,
                                       gfx::Rect clip_rect,
                                       bool is_clipped,
                                       bool contents_opaque,
                                       float draw_opacity,
                                       int sorting_context_id,
                                       gfx::Rect visible_quad_rect) {
  DCHECK(frame.get());

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  gfx::Rect rotated_size_rect(rotated_size);
  shared_quad_state->SetAll(
      transform, rotated_size_rect, visible_layer_rect, clip_rect, is_clipped,
      contents_opaque, draw_opacity, SkBlendMode::kSrcOver, sorting_context_id);

  gfx::Rect quad_rect(rotated_size);
  gfx::Rect visible_rect = frame->visible_rect();
  bool needs_blending = !contents_opaque;
  gfx::Size coded_size = frame->coded_size();

  const float tex_width_scale =
      static_cast<float>(visible_rect.width()) / coded_size.width();
  const float tex_height_scale =
      static_cast<float>(visible_rect.height()) / coded_size.height();

  switch (frame_resource_type_) {
    // TODO(danakj): Remove this, hide it in the hardware path.
    case VideoFrameExternalResources::SOFTWARE_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 0u);
      DCHECK_GT(software_resource_, viz::kInvalidResourceId);
      bool premultiplied_alpha = true;
      gfx::PointF uv_top_left(0.f, 0.f);
      gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      bool nearest_neighbor = false;
      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
          software_resource_, premultiplied_alpha, uv_top_left, uv_bottom_right,
          SK_ColorTRANSPARENT, opacity, flipped, nearest_neighbor, false);
      for (viz::ResourceId resource_id : texture_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameExternalResources::YUV_RESOURCE: {
      const gfx::Size ya_tex_size = coded_size;

      int u_width = media::VideoFrame::Columns(
          media::VideoFrame::kUPlane, frame->format(), coded_size.width());
      int u_height = media::VideoFrame::Rows(
          media::VideoFrame::kUPlane, frame->format(), coded_size.height());
      gfx::Size uv_tex_size(u_width, u_height);

      if (frame->HasTextures()) {
        if (frame->format() == media::PIXEL_FORMAT_NV12) {
          DCHECK_EQ(2u, frame_resources_.size());
        } else {
          DCHECK_EQ(media::PIXEL_FORMAT_I420, frame->format());
          DCHECK_EQ(3u,
                    frame_resources_.size());  // Alpha is not supported yet.
        }
      } else {
        DCHECK_GE(frame_resources_.size(), 3u);
        DCHECK(frame_resources_.size() <= 3 ||
               ya_tex_size == media::VideoFrame::PlaneSize(
                                  frame->format(), media::VideoFrame::kAPlane,
                                  coded_size));
      }

      // Compute the UV sub-sampling factor based on the ratio between
      // |ya_tex_size| and |uv_tex_size|.
      float uv_subsampling_factor_x =
          static_cast<float>(ya_tex_size.width()) / uv_tex_size.width();
      float uv_subsampling_factor_y =
          static_cast<float>(ya_tex_size.height()) / uv_tex_size.height();
      gfx::RectF ya_tex_coord_rect(visible_rect);
      gfx::RectF uv_tex_coord_rect(
          visible_rect.x() / uv_subsampling_factor_x,
          visible_rect.y() / uv_subsampling_factor_y,
          visible_rect.width() / uv_subsampling_factor_x,
          visible_rect.height() / uv_subsampling_factor_y);

      auto* yuv_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::YUVVideoDrawQuad>();
      yuv_video_quad->SetNew(
          shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
          ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
          frame_resources_[0].id, frame_resources_[1].id,
          frame_resources_.size() > 2 ? frame_resources_[2].id
                                      : frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[3].id : 0,
          frame->ColorSpace(), frame_resource_offset_,
          frame_resource_multiplier_, frame_bits_per_channel_);
      yuv_video_quad->require_overlay =
          frame->metadata()->IsTrue(media::VideoFrameMetadata::REQUIRE_OVERLAY);

      for (viz::ResourceId resource_id : yuv_video_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameExternalResources::RGBA_RESOURCE:
    case VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE:
    case VideoFrameExternalResources::RGB_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      bool premultiplied_alpha =
          frame_resource_type_ ==
          VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE;
      gfx::PointF uv_top_left(0.f, 0.f);
      gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      bool nearest_neighbor = false;
      auto* texture_quad =
          render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                           needs_blending, frame_resources_[0].id,
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SK_ColorTRANSPARENT, opacity, flipped,
                           nearest_neighbor, false);
      texture_quad->set_resource_size_in_pixels(coded_size);
      for (viz::ResourceId resource_id : texture_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      gfx::Transform scale;
      scale.Scale(tex_width_scale, tex_height_scale);
      auto* stream_video_quad =
          render_pass->CreateAndAppendDrawQuad<viz::StreamVideoDrawQuad>();
      stream_video_quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect,
                                needs_blending, frame_resources_[0].id,
                                frame_resources_[0].size_in_pixels, scale);
      for (viz::ResourceId resource_id : stream_video_quad->resources) {
        resource_provider_->ValidateResource(resource_id);
      }
      break;
    }
    case VideoFrameExternalResources::NONE:
      NOTIMPLEMENTED();
      break;
  }
}

VideoFrameExternalResources
VideoResourceUpdater::CreateExternalResourcesFromVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  if (video_frame->format() == media::PIXEL_FORMAT_UNKNOWN)
    return VideoFrameExternalResources();
  DCHECK(video_frame->HasTextures() || video_frame->IsMappable());
  if (video_frame->HasTextures())
    return CreateForHardwarePlanes(std::move(video_frame));
  else
    return CreateForSoftwarePlanes(std::move(video_frame));
}

VideoResourceUpdater::ResourceList::iterator
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    bool software_resource,
    int unique_id,
    int plane_index) {
  ResourceList::iterator recyclable_resource = all_resources_.end();
  for (auto it = all_resources_.begin(); it != all_resources_.end(); ++it) {
    // If the plane index is valid (positive, or 0, meaning all planes)
    // then we are allowed to return a referenced resource that already
    // contains the right frame data. It's safe to reuse it even if
    // resource_provider_ holds some references to it, because those
    // references are read-only.
    if (plane_index != -1 && it->Matches(unique_id, plane_index)) {
      DCHECK(it->resource_size() == resource_size);
      DCHECK(it->resource_format() == resource_format);
      DCHECK(it->mailbox().IsZero() == software_resource);
      return it;
    }

    // Otherwise check whether this is an unreferenced resource of the right
    // format that we can recycle. Remember it, but don't return immediately,
    // because we still want to find any reusable resources.
    // Resources backed by SharedMemory are not ref-counted, unlike mailboxes,
    // so the definition of |in_use| must take this into account. Full
    // discussion in codereview.chromium.org/145273021.
    const bool in_use =
        it->has_refs() ||
        (software_resource &&
         resource_provider_->InUseByConsumer(it->resource_id()));

    if (!in_use && it->resource_size() == resource_size &&
        it->resource_format() == resource_format &&
        it->mailbox().IsZero() == software_resource) {
      recyclable_resource = it;
    }
  }

  if (recyclable_resource != all_resources_.end())
    return recyclable_resource;

  // There was nothing available to reuse or recycle. Allocate a new resource.
  return AllocateResource(resource_size, resource_format, color_space,
                          software_resource);
}

VideoResourceUpdater::ResourceList::iterator
VideoResourceUpdater::AllocateResource(const gfx::Size& plane_size,
                                       viz::ResourceFormat format,
                                       const gfx::ColorSpace& color_space,
                                       bool software_resource) {
  viz::ResourceId resource_id;
  gpu::Mailbox mailbox;

  // TODO(danakj): Abstract out hw/sw resource create/delete from
  // ResourceProvider and stop using ResourceProvider in this class.
  if (software_resource) {
    DCHECK_EQ(format, viz::RGBA_8888);
    resource_id = resource_provider_->CreateBitmapResource(
        plane_size, color_space, viz::RGBA_8888);
  } else {
    DCHECK(context_provider_);

    resource_id = resource_provider_->CreateGpuTextureResource(
        plane_size, viz::ResourceTextureHint::kDefault, format, color_space);

    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    gl->GenMailboxCHROMIUM(mailbox.name);
    LayerTreeResourceProvider::ScopedWriteLockGL lock(resource_provider_,
                                                      resource_id);
    gl->ProduceTextureDirectCHROMIUM(
        lock.GetTexture(),
        mailbox.name);
  }
  all_resources_.emplace_front(resource_id, plane_size, format, mailbox);
  return all_resources_.begin();
}

void VideoResourceUpdater::DeleteResource(ResourceList::iterator resource_it) {
  DCHECK(!resource_it->has_refs());
  resource_provider_->DeleteResource(resource_it->resource_id());
  all_resources_.erase(resource_it);
}

void VideoResourceUpdater::CopyHardwarePlane(
    media::VideoFrame* video_frame,
    const gfx::ColorSpace& resource_color_space,
    const gpu::MailboxHolder& mailbox_holder,
    VideoFrameExternalResources* external_resources) {
  const gfx::Size output_plane_resource_size = video_frame->coded_size();
  // The copy needs to be a direct transfer of pixel data, so we use an RGBA8
  // target to avoid loss of precision or dropping any alpha component.
  const viz::ResourceFormat copy_target_format = viz::ResourceFormat::RGBA_8888;

  const int no_unique_id = 0;
  const int no_plane_index = -1;  // Do not recycle referenced textures.
  VideoResourceUpdater::ResourceList::iterator resource =
      RecycleOrAllocateResource(output_plane_resource_size, copy_target_format,
                                resource_color_space, false, no_unique_id,
                                no_plane_index);
  resource->add_ref();

  LayerTreeResourceProvider::ScopedWriteLockGL lock(resource_provider_,
                                                    resource->resource_id());
  DCHECK_EQ(
      resource_provider_->GetResourceTextureTarget(resource->resource_id()),
      (GLenum)GL_TEXTURE_2D);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  uint32_t src_texture_id =
      gl->CreateAndConsumeTextureCHROMIUM(mailbox_holder.mailbox.name);
  gl->CopySubTextureCHROMIUM(
      src_texture_id, 0, GL_TEXTURE_2D, lock.GetTexture(), 0, 0, 0, 0, 0,
      output_plane_resource_size.width(), output_plane_resource_size.height(),
      false, false, false);
  gl->DeleteTextures(1, &src_texture_id);

  // Pass an empty sync token to force generation of a new sync token.
  SyncTokenClientImpl client(gl, gpu::SyncToken());
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);

  auto transfer_resource = viz::TransferableResource::MakeGL(
      resource->mailbox(), GL_LINEAR, GL_TEXTURE_2D, sync_token);
  transfer_resource.color_space = resource_color_space;
  external_resources->resources.push_back(std::move(transfer_resource));

  external_resources->release_callbacks.push_back(
      base::BindOnce(&VideoResourceUpdater::RecycleResource,
                     weak_ptr_factory_.GetWeakPtr(), resource->resource_id()));
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForHardwarePlanes");
  DCHECK(video_frame->HasTextures());
  if (!context_provider_)
    return VideoFrameExternalResources();

  VideoFrameExternalResources external_resources;
  gfx::ColorSpace resource_color_space = video_frame->ColorSpace();

  bool copy_required =
      video_frame->metadata()->IsTrue(media::VideoFrameMetadata::COPY_REQUIRED);

  GLuint target = video_frame->mailbox_holder(0).texture_target;
  // If |copy_required| then we will copy into a GL_TEXTURE_2D target.
  if (copy_required)
    target = GL_TEXTURE_2D;

  gfx::BufferFormat buffer_format;
  external_resources.type = ExternalResourceTypeForHardwarePlanes(
      video_frame->format(), target, video_frame->NumTextures(), &buffer_format,
      use_stream_video_draw_quad_);
  if (external_resources.type == VideoFrameExternalResources::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << media::VideoPixelFormatToString(video_frame->format());
    return external_resources;
  }
  if (external_resources.type == VideoFrameExternalResources::RGB_RESOURCE ||
      external_resources.type == VideoFrameExternalResources::RGBA_RESOURCE ||
      external_resources.type ==
          VideoFrameExternalResources::RGBA_PREMULTIPLIED_RESOURCE) {
    resource_color_space = resource_color_space.GetAsFullRangeRGB();
  }

  const size_t num_textures = video_frame->NumTextures();
  for (size_t i = 0; i < num_textures; ++i) {
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    if (mailbox_holder.mailbox.IsZero())
      break;

    if (copy_required) {
      CopyHardwarePlane(video_frame.get(), resource_color_space, mailbox_holder,
                        &external_resources);
    } else {
      auto transfer_resource = viz::TransferableResource::MakeGLOverlay(
          mailbox_holder.mailbox, GL_LINEAR, mailbox_holder.texture_target,
          mailbox_holder.sync_token, video_frame->coded_size(),
          video_frame->metadata()->IsTrue(
              media::VideoFrameMetadata::ALLOW_OVERLAY));
      transfer_resource.color_space = resource_color_space;
      transfer_resource.read_lock_fences_enabled =
          video_frame->metadata()->IsTrue(
              media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED);
      transfer_resource.buffer_format = buffer_format;
#if defined(OS_ANDROID)
      transfer_resource.is_backed_by_surface_texture =
          video_frame->metadata()->IsTrue(
              media::VideoFrameMetadata::SURFACE_TEXTURE);
      transfer_resource.wants_promotion_hint = video_frame->metadata()->IsTrue(
          media::VideoFrameMetadata::WANTS_PROMOTION_HINT);
#endif
      external_resources.resources.push_back(std::move(transfer_resource));
      external_resources.release_callbacks.push_back(
          base::BindOnce(&VideoResourceUpdater::ReturnTexture,
                         weak_ptr_factory_.GetWeakPtr(), video_frame));
    }
  }
  return external_resources;
}

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForSoftwarePlanes");
  const media::VideoPixelFormat input_frame_format = video_frame->format();

  size_t bits_per_channel = video_frame->BitDepth();

  // Only YUV and Y16 software video frames are supported.
  DCHECK(media::IsYuvPlanar(input_frame_format) ||
         input_frame_format == media::PIXEL_FORMAT_Y16);

  const bool software_compositor = context_provider_ == nullptr;

  viz::ResourceFormat output_resource_format;
  gfx::ColorSpace output_color_space = video_frame->ColorSpace();
  if (input_frame_format == media::PIXEL_FORMAT_Y16) {
    // Unable to display directly as yuv planes so convert it to RGBA for
    // compositing.
    output_resource_format = viz::RGBA_8888;
    output_color_space = output_color_space.GetAsFullRangeRGB();
  } else {
    // Can be composited directly from yuv planes.
    output_resource_format =
        resource_provider_->YuvResourceFormat(bits_per_channel);
  }

  // If GPU compositing is enabled, but the output resource format
  // returned by the resource provider is viz::RGBA_8888, then a GPU driver
  // bug workaround requires that YUV frames must be converted to RGB
  // before texture upload.
  bool texture_needs_rgb_conversion =
      !software_compositor &&
      output_resource_format == viz::ResourceFormat::RGBA_8888;

  size_t output_plane_count = media::VideoFrame::NumPlanes(input_frame_format);

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor || texture_needs_rgb_conversion) {
    output_resource_format = viz::RGBA_8888;
    output_plane_count = 1;
    bits_per_channel = 8;

    // The YUV to RGB conversion will be performed when we convert
    // from single-channel textures to an RGBA texture via
    // ConvertVideoFrameToRGBPixels below.
    output_color_space = output_color_space.GetAsFullRangeRGB();
  }

  // Drop recycled resources that are the wrong format.
  for (auto it = all_resources_.begin(); it != all_resources_.end();) {
    if (!it->has_refs() && it->resource_format() != output_resource_format)
      DeleteResource(it++);
    else
      ++it;
  }

  const int max_resource_size = resource_provider_->max_texture_size();
  std::vector<ResourceList::iterator> plane_resources;
  for (size_t i = 0; i < output_plane_count; ++i) {
    gfx::Size output_plane_resource_size =
        SoftwarePlaneDimension(video_frame.get(), software_compositor, i);
    if (output_plane_resource_size.IsEmpty() ||
        output_plane_resource_size.width() > max_resource_size ||
        output_plane_resource_size.height() > max_resource_size) {
      // This output plane has invalid geometry. Clean up and return an empty
      // external resources.
      for (ResourceList::iterator resource_it : plane_resources)
        resource_it->remove_ref();
      return VideoFrameExternalResources();
    }

    ResourceList::iterator resource_it = RecycleOrAllocateResource(
        output_plane_resource_size, output_resource_format, output_color_space,
        software_compositor, video_frame->unique_id(), i);

    resource_it->add_ref();
    plane_resources.push_back(resource_it);
  }

  VideoFrameExternalResources external_resources;

  external_resources.bits_per_channel = bits_per_channel;

  if (software_compositor || texture_needs_rgb_conversion) {
    DCHECK_EQ(plane_resources.size(), 1u);
    PlaneResource& plane_resource = *plane_resources[0];
    DCHECK_EQ(plane_resource.resource_format(), viz::RGBA_8888);
    DCHECK(!software_compositor ||
           plane_resource.resource_id() > viz::kInvalidResourceId);
    DCHECK_EQ(software_compositor, plane_resource.mailbox().IsZero());

    if (!plane_resource.Matches(video_frame->unique_id(), 0)) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (software_compositor) {
        if (!video_renderer_)
          video_renderer_ = std::make_unique<media::PaintCanvasVideoRenderer>();

        LayerTreeResourceProvider::ScopedWriteLockSoftware lock(
            resource_provider_, plane_resource.resource_id());
        SkiaPaintCanvas canvas(lock.sk_bitmap());
        // This is software path, so canvas and video_frame are always backed
        // by software.
        video_renderer_->Copy(video_frame, &canvas, media::Context3D());
      } else {
        size_t bytes_per_row = viz::ResourceSizes::CheckedWidthInBytes<size_t>(
            video_frame->coded_size().width(), viz::ResourceFormat::RGBA_8888);
        size_t needed_size = bytes_per_row * video_frame->coded_size().height();
        if (upload_pixels_.size() < needed_size) {
          // Clear before resizing to avoid memcpy.
          upload_pixels_.clear();
          upload_pixels_.resize(needed_size);
        }

        media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
            video_frame.get(), &upload_pixels_[0], bytes_per_row);

        resource_provider_->CopyToResource(plane_resource.resource_id(),
                                           &upload_pixels_[0],
                                           plane_resource.resource_size());
      }
      plane_resource.SetUniqueId(video_frame->unique_id(), 0);
    }

    if (software_compositor) {
      external_resources.software_resource = plane_resource.resource_id();
      external_resources.software_release_callback = base::BindOnce(
          &VideoResourceUpdater::RecycleResource,
          weak_ptr_factory_.GetWeakPtr(), plane_resource.resource_id());
      external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;
    } else {
      gpu::SyncToken sync_token;
      GenerateCompositorSyncToken(context_provider_->ContextGL(), &sync_token);

      GLuint target = resource_provider_->GetResourceTextureTarget(
          plane_resource.resource_id());
      auto transfer_resource = viz::TransferableResource::MakeGL(
          plane_resource.mailbox(), GL_LINEAR, target, sync_token);
      transfer_resource.color_space = output_color_space;

      external_resources.resources.push_back(std::move(transfer_resource));
      external_resources.release_callbacks.push_back(base::BindOnce(
          &VideoResourceUpdater::RecycleResource,
          weak_ptr_factory_.GetWeakPtr(), plane_resource.resource_id()));
      external_resources.type = VideoFrameExternalResources::RGBA_RESOURCE;
    }
    return external_resources;
  }

  const viz::ResourceFormat yuv_resource_format =
      resource_provider_->YuvResourceFormat(bits_per_channel);
  DCHECK(yuv_resource_format == viz::LUMINANCE_F16 ||
         yuv_resource_format == viz::R16_EXT ||
         yuv_resource_format == viz::LUMINANCE_8 ||
         yuv_resource_format == viz::RED_8)
      << yuv_resource_format;

  std::unique_ptr<media::HalfFloatMaker> half_float_maker;
  if (yuv_resource_format == viz::LUMINANCE_F16) {
    half_float_maker =
        media::HalfFloatMaker::NewHalfFloatMaker(bits_per_channel);
    external_resources.offset = half_float_maker->Offset();
    external_resources.multiplier = half_float_maker->Multiplier();
  } else if (yuv_resource_format == viz::R16_EXT) {
    external_resources.multiplier = 65535.0f / ((1 << bits_per_channel) - 1);
    external_resources.offset = 0;
  }

  // We need to transfer data from |video_frame| to the plane resources.
  for (size_t i = 0; i < plane_resources.size(); ++i) {
    PlaneResource& plane_resource = *plane_resources[i];
    // Skip the transfer if this |video_frame|'s plane has been processed.
    if (plane_resource.Matches(video_frame->unique_id(), i))
      continue;

    const viz::ResourceFormat plane_resource_format =
        plane_resource.resource_format();
    DCHECK_EQ(plane_resource_format, yuv_resource_format);

    // TODO(hubbe): Move upload code to media/.
    // TODO(reveman): Can use GpuMemoryBuffers here to improve performance.

    // |video_stride_bytes| is the width of the |video_frame| we are uploading
    // (including non-frame data to fill in the stride).
    const int video_stride_bytes = video_frame->stride(i);

    // |resource_size_pixels| is the size of the destination resource.
    const gfx::Size resource_size_pixels = plane_resource.resource_size();

    const size_t bytes_per_row =
        viz::ResourceSizes::CheckedWidthInBytes<size_t>(
            resource_size_pixels.width(), plane_resource_format);
    // Use 4-byte row alignment (OpenGL default) for upload performance.
    // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
    const size_t upload_image_stride =
        MathUtil::CheckedRoundUp<size_t>(bytes_per_row, 4u);

    const size_t resource_bit_depth =
        static_cast<size_t>(viz::BitsPerPixel(plane_resource_format));

    // Data downshifting is needed if the resource bit depth is not enough.
    const bool needs_bit_downshifting = bits_per_channel > resource_bit_depth;

    // A copy to adjust strides is needed if those are different and both source
    // and destination have the same bit depth.
    const bool needs_stride_adaptation =
        (bits_per_channel == resource_bit_depth) &&
        (upload_image_stride != static_cast<size_t>(video_stride_bytes));

    // We need to convert the incoming data if we're transferring to half float,
    // if the need a bit downshift or if the strides need to be reconciled.
    const bool needs_conversion = plane_resource_format == viz::LUMINANCE_F16 ||
                                  needs_bit_downshifting ||
                                  needs_stride_adaptation;

    const uint8_t* pixels;
    if (!needs_conversion) {
      pixels = video_frame->data(i);
    } else {
      // Avoid malloc for each frame/plane if possible.
      const size_t needed_size =
          upload_image_stride * resource_size_pixels.height();
      if (upload_pixels_.size() < needed_size) {
        // Clear before resizing to avoid memcpy.
        upload_pixels_.clear();
        upload_pixels_.resize(needed_size);
      }

      if (plane_resource_format == viz::LUMINANCE_F16) {
        for (int row = 0; row < resource_size_pixels.height(); ++row) {
          uint16_t* dst = reinterpret_cast<uint16_t*>(
              &upload_pixels_[upload_image_stride * row]);
          const uint16_t* src = reinterpret_cast<uint16_t*>(
              video_frame->data(i) + (video_stride_bytes * row));
          half_float_maker->MakeHalfFloats(src, bytes_per_row / 2, dst);
        }
      } else if (needs_bit_downshifting) {
        DCHECK(plane_resource_format == viz::LUMINANCE_8 ||
               plane_resource_format == viz::RED_8);
        const int scale = 0x10000 >> (bits_per_channel - 8);
        libyuv::Convert16To8Plane(
            reinterpret_cast<uint16_t*>(video_frame->data(i)),
            video_stride_bytes / 2, upload_pixels_.data(), upload_image_stride,
            scale, bytes_per_row, resource_size_pixels.height());
      } else {
        // Make a copy to reconcile stride, size and format being equal.
        DCHECK(needs_stride_adaptation);
        DCHECK(plane_resource_format == viz::LUMINANCE_8 ||
               plane_resource_format == viz::RED_8);
        libyuv::CopyPlane(video_frame->data(i), video_stride_bytes,
                          upload_pixels_.data(), upload_image_stride,
                          resource_size_pixels.width(),
                          resource_size_pixels.height());
      }

      pixels = &upload_pixels_[0];
    }

    resource_provider_->CopyToResource(plane_resource.resource_id(), pixels,
                                       resource_size_pixels);
    plane_resource.SetUniqueId(video_frame->unique_id(), i);
  }

  // Set the sync token otherwise resource is assumed to be synchronized.
  gpu::SyncToken sync_token;
  GenerateCompositorSyncToken(context_provider_->ContextGL(), &sync_token);

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    PlaneResource& plane_resource = *plane_resources[i];
    GLuint target = resource_provider_->GetResourceTextureTarget(
        plane_resource.resource_id());
    auto transfer_resource = viz::TransferableResource::MakeGL(
        plane_resource.mailbox(), GL_LINEAR, target, sync_token);
    transfer_resource.color_space = output_color_space;
    external_resources.resources.push_back(std::move(transfer_resource));
    external_resources.release_callbacks.push_back(base::BindOnce(
        &VideoResourceUpdater::RecycleResource, weak_ptr_factory_.GetWeakPtr(),
        plane_resource.resource_id()));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

void VideoResourceUpdater::ReturnTexture(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  // TODO(dshwang): Forward to the decoder as a lost resource.
  if (lost_resource)
    return;

  // The video frame will insert a wait on the previous release sync token.
  SyncTokenClientImpl client(context_provider_->ContextGL(), sync_token);
  video_frame->UpdateReleaseSyncToken(&client);
}

void VideoResourceUpdater::RecycleResource(viz::ResourceId resource_id,
                                           const gpu::SyncToken& sync_token,
                                           bool lost_resource) {
  auto resource_it =
      std::find_if(all_resources_.begin(), all_resources_.end(),
                   [resource_id](const PlaneResource& plane_resource) {
                     return plane_resource.resource_id() == resource_id;
                   });
  if (resource_it == all_resources_.end())
    return;

  if (context_provider_ && sync_token.HasData()) {
    context_provider_->ContextGL()->WaitSyncTokenCHROMIUM(
        sync_token.GetConstData());
  }

  if (lost_resource) {
    resource_it->clear_refs();
    DeleteResource(resource_it);
  } else {
    resource_it->remove_ref();
  }
}

}  // namespace cc
