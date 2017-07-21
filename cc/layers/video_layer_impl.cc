// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/single_release_callback_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/task_runner_provider.h"
#include "media/base/video_frame.h"
#include "ui/gfx/color_space.h"

namespace cc {

// static
std::unique_ptr<VideoLayerImpl> VideoLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    VideoFrameProvider* provider,
    media::VideoRotation video_rotation) {
  DCHECK(tree_impl->task_runner_provider()->IsMainThreadBlocked());
  DCHECK(tree_impl->task_runner_provider()->IsImplThread());

  scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl =
      VideoFrameProviderClientImpl::Create(
          provider, tree_impl->GetVideoFrameControllerClient());

  return base::WrapUnique(new VideoLayerImpl(
      tree_impl, id, std::move(provider_client_impl), video_rotation));
}

VideoLayerImpl::VideoLayerImpl(
    LayerTreeImpl* tree_impl,
    int id,
    scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl,
    media::VideoRotation video_rotation)
    : LayerImpl(tree_impl, id),
      provider_client_impl_(std::move(provider_client_impl)),
      frame_(nullptr),
      video_rotation_(video_rotation) {
  set_may_contain_video(true);
}

VideoLayerImpl::~VideoLayerImpl() {
  if (!provider_client_impl_->Stopped()) {
    // In impl side painting, we may have a pending and active layer
    // associated with the video provider at the same time. Both have a ref
    // on the VideoFrameProviderClientImpl, but we stop when the first
    // LayerImpl (the one on the pending tree) is destroyed since we know
    // the main thread is blocked for this commit.
    DCHECK(layer_tree_impl()->task_runner_provider()->IsImplThread());
    DCHECK(layer_tree_impl()->task_runner_provider()->IsMainThreadBlocked());
    provider_client_impl_->Stop();
  }
}

std::unique_ptr<LayerImpl> VideoLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return base::WrapUnique(new VideoLayerImpl(
      tree_impl, id(), provider_client_impl_, video_rotation_));
}

void VideoLayerImpl::DidBecomeActive() {
  provider_client_impl_->SetActiveVideoLayer(this);
}

bool VideoLayerImpl::WillDraw(DrawMode draw_mode,
                              ResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;

  // Explicitly acquire and release the provider mutex so it can be held from
  // WillDraw to DidDraw. Since the compositor thread is in the middle of
  // drawing, the layer will not be destroyed before DidDraw is called.
  // Therefore, the only thing that will prevent this lock from being released
  // is the GPU process locking it. As the GPU process can't cause the
  // destruction of the provider (calling StopUsingProvider), holding this
  // lock should not cause a deadlock.
  frame_ = provider_client_impl_->AcquireLockAndCurrentFrame();

  if (!frame_.get()) {
    // Drop any resources used by the updater if there is no frame to display.
    updater_ = nullptr;

    provider_client_impl_->ReleaseLock();
    return false;
  }

  if (!LayerImpl::WillDraw(draw_mode, resource_provider))
    return false;

  if (!updater_) {
    updater_.reset(new VideoResourceUpdater(
        layer_tree_impl()->context_provider(),
        layer_tree_impl()->resource_provider(),
        layer_tree_impl()->settings().use_stream_video_draw_quad));
  }

  VideoFrameExternalResources external_resources =
      updater_->CreateExternalResourcesFromVideoFrame(frame_);
  frame_resource_type_ = external_resources.type;

  if (external_resources.type ==
      VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    software_resources_ = external_resources.software_resources;
    software_release_callback_ =
        external_resources.software_release_callback;
    return true;
  }
  frame_resource_offset_ = external_resources.offset;
  frame_resource_multiplier_ = external_resources.multiplier;
  frame_bits_per_channel_ = external_resources.bits_per_channel;

  DCHECK_EQ(external_resources.mailboxes.size(),
            external_resources.release_callbacks.size());
  ResourceProvider::ResourceIdArray resource_ids;
  resource_ids.reserve(external_resources.mailboxes.size());
  for (size_t i = 0; i < external_resources.mailboxes.size(); ++i) {
    unsigned resource_id = resource_provider->CreateResourceFromTextureMailbox(
        external_resources.mailboxes[i],
        SingleReleaseCallbackImpl::Create(
            external_resources.release_callbacks[i]),
        external_resources.read_lock_fences_enabled);
    frame_resources_.push_back(FrameResource(
        resource_id, external_resources.mailboxes[i].size_in_pixels(),
        external_resources.mailboxes[i].is_overlay_candidate()));
    resource_ids.push_back(resource_id);
  }
  resource_provider->GenerateSyncTokenForResources(resource_ids);

  return true;
}

void VideoLayerImpl::AppendQuads(RenderPass* render_pass,
                                 AppendQuadsData* append_quads_data) {
  DCHECK(frame_.get());

  gfx::Transform transform = DrawTransform();
  gfx::Size rotated_size = bounds();

  switch (video_rotation_) {
    case media::VIDEO_ROTATION_90:
      rotated_size = gfx::Size(rotated_size.height(), rotated_size.width());
      transform.Rotate(90.0);
      transform.Translate(0.0, -rotated_size.height());
      break;
    case media::VIDEO_ROTATION_180:
      transform.Rotate(180.0);
      transform.Translate(-rotated_size.width(), -rotated_size.height());
      break;
    case media::VIDEO_ROTATION_270:
      rotated_size = gfx::Size(rotated_size.height(), rotated_size.width());
      transform.Rotate(270.0);
      transform.Translate(-rotated_size.width(), 0);
    case media::VIDEO_ROTATION_0:
      break;
  }

  SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(transform, gfx::Rect(rotated_size),
                            visible_layer_rect(), clip_rect(), is_clipped(),
                            draw_opacity(), SkBlendMode::kSrcOver,
                            GetSortingContextId());

  AppendDebugBorderQuad(
      render_pass, rotated_size, shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(rotated_size);
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  gfx::Rect visible_rect = frame_->visible_rect();
  gfx::Size coded_size = frame_->coded_size();

  Occlusion occlusion_in_video_space =
      draw_properties()
          .occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
              transform);
  gfx::Rect visible_quad_rect =
      occlusion_in_video_space.GetUnoccludedContentRect(quad_rect);
  if (visible_quad_rect.IsEmpty())
    return;

  const float tex_width_scale =
      static_cast<float>(visible_rect.width()) / coded_size.width();
  const float tex_height_scale =
      static_cast<float>(visible_rect.height()) / coded_size.height();

  switch (frame_resource_type_) {
    // TODO(danakj): Remove this, hide it in the hardware path.
    case VideoFrameExternalResources::SOFTWARE_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 0u);
      DCHECK_EQ(software_resources_.size(), 1u);
      if (software_resources_.size() < 1u)
        break;
      bool premultiplied_alpha = true;
      gfx::PointF uv_top_left(0.f, 0.f);
      gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      bool nearest_neighbor = false;
      TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, opaque_rect,
                           visible_quad_rect, software_resources_[0],
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SK_ColorTRANSPARENT, opacity, flipped,
                           nearest_neighbor, false);
      ValidateQuadResources(texture_quad);
      break;
    }
    case VideoFrameExternalResources::YUV_RESOURCE: {
      YUVVideoDrawQuad::ColorSpace color_space = YUVVideoDrawQuad::REC_601;
      int videoframe_color_space;
      if (frame_->metadata()->GetInteger(media::VideoFrameMetadata::COLOR_SPACE,
                                         &videoframe_color_space)) {
        if (videoframe_color_space == media::COLOR_SPACE_JPEG) {
          color_space = YUVVideoDrawQuad::JPEG;
        } else if (videoframe_color_space == media::COLOR_SPACE_HD_REC709) {
          color_space = YUVVideoDrawQuad::REC_709;
        }
      }

      const gfx::Size ya_tex_size = coded_size;
      gfx::Size uv_tex_size = media::VideoFrame::PlaneSize(
          frame_->format(), media::VideoFrame::kUPlane, coded_size);

      if (frame_->HasTextures()) {
        if (frame_->format() == media::PIXEL_FORMAT_NV12) {
          DCHECK_EQ(2u, frame_resources_.size());
        } else {
          DCHECK_EQ(media::PIXEL_FORMAT_I420, frame_->format());
          DCHECK_EQ(3u,
                    frame_resources_.size());  // Alpha is not supported yet.
        }
      } else {
        DCHECK(uv_tex_size ==
               media::VideoFrame::PlaneSize(
                   frame_->format(), media::VideoFrame::kVPlane, coded_size));
        DCHECK_GE(frame_resources_.size(), 3u);
        DCHECK(frame_resources_.size() <= 3 ||
               ya_tex_size == media::VideoFrame::PlaneSize(
                                  frame_->format(), media::VideoFrame::kAPlane,
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

      YUVVideoDrawQuad* yuv_video_quad =
          render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
      yuv_video_quad->SetNew(
          shared_quad_state, quad_rect, opaque_rect, visible_quad_rect,
          ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size, uv_tex_size,
          frame_resources_[0].id, frame_resources_[1].id,
          frame_resources_.size() > 2 ? frame_resources_[2].id
                                      : frame_resources_[1].id,
          frame_resources_.size() > 3 ? frame_resources_[3].id : 0, color_space,
          frame_->ColorSpace(), frame_resource_offset_,
          frame_resource_multiplier_, frame_bits_per_channel_);
      yuv_video_quad->require_overlay = frame_->metadata()->IsTrue(
          media::VideoFrameMetadata::REQUIRE_OVERLAY);
      ValidateQuadResources(yuv_video_quad);
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
      TextureDrawQuad* texture_quad =
          render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_quad->SetNew(shared_quad_state, quad_rect, opaque_rect,
                           visible_quad_rect, frame_resources_[0].id,
                           premultiplied_alpha, uv_top_left, uv_bottom_right,
                           SK_ColorTRANSPARENT, opacity, flipped,
                           nearest_neighbor, false);
      texture_quad->set_resource_size_in_pixels(coded_size);
      ValidateQuadResources(texture_quad);
      break;
    }
    case VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 1u);
      if (frame_resources_.size() < 1u)
        break;
      gfx::Transform scale;
      scale.Scale(tex_width_scale, tex_height_scale);
      StreamVideoDrawQuad* stream_video_quad =
          render_pass->CreateAndAppendDrawQuad<StreamVideoDrawQuad>();
      stream_video_quad->SetNew(shared_quad_state, quad_rect, opaque_rect,
                                visible_quad_rect, frame_resources_[0].id,
                                frame_resources_[0].size_in_pixels, scale);
      ValidateQuadResources(stream_video_quad);
      break;
    }
    case VideoFrameExternalResources::NONE:
      NOTIMPLEMENTED();
      break;
  }
}

void VideoLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  DCHECK(frame_.get());

  if (frame_resource_type_ ==
      VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    for (size_t i = 0; i < software_resources_.size(); ++i) {
      software_release_callback_.Run(gpu::SyncToken(), false,
                                     layer_tree_impl()
                                         ->task_runner_provider()
                                         ->blocking_main_thread_task_runner());
    }

    software_resources_.clear();
    software_release_callback_.Reset();
  } else {
    for (size_t i = 0; i < frame_resources_.size(); ++i)
      resource_provider->DeleteResource(frame_resources_[i].id);
    frame_resources_.clear();
  }

  provider_client_impl_->PutCurrentFrame();
  frame_ = nullptr;

  provider_client_impl_->ReleaseLock();
}

SimpleEnclosedRegion VideoLayerImpl::VisibleOpaqueRegion() const {
  // If we don't have a frame yet, then we don't have an opaque region.
  if (!provider_client_impl_->HasCurrentFrame())
    return SimpleEnclosedRegion();
  return LayerImpl::VisibleOpaqueRegion();
}

void VideoLayerImpl::ReleaseResources() {
  updater_ = nullptr;
}

void VideoLayerImpl::SetNeedsRedraw() {
  SetUpdateRect(gfx::UnionRects(update_rect(), gfx::Rect(bounds())));
  layer_tree_impl()->SetNeedsRedraw();
}

const char* VideoLayerImpl::LayerTypeAsString() const {
  return "cc::VideoLayerImpl";
}

}  // namespace cc
