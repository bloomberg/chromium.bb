// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/proxy.h"
#include "media/base/video_frame.h"

#if defined(GOOGLE_TV)
#include "cc/quads/solid_color_draw_quad.h"
#endif

namespace cc {

// static
scoped_ptr<VideoLayerImpl> VideoLayerImpl::Create(
    LayerTreeImpl* tree_impl,
    int id,
    VideoFrameProvider* provider) {
  scoped_ptr<VideoLayerImpl> layer(new VideoLayerImpl(tree_impl, id));
  layer->SetProviderClientImpl(VideoFrameProviderClientImpl::Create(provider));
  DCHECK(tree_impl->proxy()->IsImplThread());
  DCHECK(tree_impl->proxy()->IsMainThreadBlocked());
  return layer.Pass();
}

VideoLayerImpl::VideoLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id),
      frame_(NULL),
      hardware_resource_(0) {}

VideoLayerImpl::~VideoLayerImpl() {
  if (!provider_client_impl_->Stopped()) {
    // In impl side painting, we may have a pending and active layer
    // associated with the video provider at the same time. Both have a ref
    // on the VideoFrameProviderClientImpl, but we stop when the first
    // LayerImpl (the one on the pending tree) is destroyed since we know
    // the main thread is blocked for this commit.
    DCHECK(layer_tree_impl()->proxy()->IsImplThread());
    DCHECK(layer_tree_impl()->proxy()->IsMainThreadBlocked());
    provider_client_impl_->Stop();
  }
}

scoped_ptr<LayerImpl> VideoLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return scoped_ptr<LayerImpl>(new VideoLayerImpl(tree_impl, id()));
}

void VideoLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);

  VideoLayerImpl* other = static_cast<VideoLayerImpl*>(layer);
  other->SetProviderClientImpl(provider_client_impl_);
}

void VideoLayerImpl::DidBecomeActive() {
  provider_client_impl_->set_active_video_layer(this);
}

void VideoLayerImpl::WillDraw(ResourceProvider* resource_provider) {
  LayerImpl::WillDraw(resource_provider);

  // Explicitly acquire and release the provider mutex so it can be held from
  // WillDraw to DidDraw. Since the compositor thread is in the middle of
  // drawing, the layer will not be destroyed before DidDraw is called.
  // Therefore, the only thing that will prevent this lock from being released
  // is the GPU process locking it. As the GPU process can't cause the
  // destruction of the provider (calling StopUsingProvider), holding this
  // lock should not cause a deadlock.
  frame_ = provider_client_impl_->AcquireLockAndCurrentFrame();

  if (!frame_) {
    // Drop any resources used by the updater if there is no frame to display.
    updater_.reset();

    provider_client_impl_->ReleaseLock();
    return;
  }

  if (!updater_)
    updater_.reset(new VideoResourceUpdater(resource_provider));

  VideoFrameExternalResources external_resources;
  if (frame_->format() == media::VideoFrame::NATIVE_TEXTURE)
    external_resources = updater_->CreateForHardwarePlanes(frame_);
  else
    external_resources = updater_->CreateForSoftwarePlanes(frame_);

  frame_resource_type_ = external_resources.type;

  if (external_resources.type ==
      VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    software_resources_ = external_resources.software_resources;
    software_release_callback_ =
        external_resources.software_release_callback;
    return;
  }

  if (external_resources.hardware_resource) {
    hardware_resource_ = external_resources.hardware_resource;
    hardware_release_callback_ =
        external_resources.hardware_release_callback;
    return;
  }

  for (size_t i = 0; i < external_resources.mailboxes.size(); ++i) {
    frame_resources_.push_back(
        resource_provider->CreateResourceFromTextureMailbox(
            external_resources.mailboxes[i]));
  }
}

void VideoLayerImpl::AppendQuads(QuadSink* quad_sink,
                                 AppendQuadsData* append_quads_data) {
  if (!frame_)
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  gfx::Rect visible_rect = frame_->visible_rect();
  gfx::Size coded_size = frame_->coded_size();

  // Pixels for macroblocked formats.
  float tex_width_scale =
      static_cast<float>(visible_rect.width()) / coded_size.width();
  float tex_height_scale =
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
      scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
      texture_quad->SetNew(shared_quad_state,
                           quad_rect,
                           opaque_rect,
                           software_resources_[0],
                           premultiplied_alpha,
                           uv_top_left,
                           uv_bottom_right,
                           opacity,
                           flipped);
      quad_sink->Append(texture_quad.PassAs<DrawQuad>(), append_quads_data);
      break;
    }
    case VideoFrameExternalResources::YUV_RESOURCE: {
      DCHECK_EQ(frame_resources_.size(), 3u);
      if (frame_resources_.size() < 3u)
        break;
      gfx::SizeF tex_scale(tex_width_scale, tex_height_scale);
      scoped_ptr<YUVVideoDrawQuad> yuv_video_quad = YUVVideoDrawQuad::Create();
      yuv_video_quad->SetNew(shared_quad_state,
                             quad_rect,
                             opaque_rect,
                             tex_scale,
                             frame_resources_[0],
                             frame_resources_[1],
                             frame_resources_[2]);
      quad_sink->Append(yuv_video_quad.PassAs<DrawQuad>(), append_quads_data);
      break;
    }
    case VideoFrameExternalResources::RGB_RESOURCE: {
      if (!hardware_resource_) {
        DCHECK_EQ(frame_resources_.size(), 1u);
        if (frame_resources_.size() < 1u)
          break;
      }
      bool premultiplied_alpha = true;
      gfx::PointF uv_top_left(0.f, 0.f);
      gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
      texture_quad->SetNew(shared_quad_state,
                           quad_rect,
                           opaque_rect,
                           hardware_resource_ ? hardware_resource_
                                              : frame_resources_[0],
                           premultiplied_alpha,
                           uv_top_left,
                           uv_bottom_right,
                           opacity,
                           flipped);
      quad_sink->Append(texture_quad.PassAs<DrawQuad>(), append_quads_data);
      break;
    }
    case VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE: {
      if (!hardware_resource_) {
        DCHECK_EQ(frame_resources_.size(), 1u);
        if (frame_resources_.size() < 1u)
          break;
      }
      gfx::Transform transform(
          provider_client_impl_->stream_texture_matrix());
      transform.Scale(tex_width_scale, tex_height_scale);
      scoped_ptr<StreamVideoDrawQuad> stream_video_quad =
          StreamVideoDrawQuad::Create();
      stream_video_quad->SetNew(shared_quad_state,
                                quad_rect,
                                opaque_rect,
                                hardware_resource_ ? hardware_resource_
                                                   : frame_resources_[0],
                                transform);
      quad_sink->Append(stream_video_quad.PassAs<DrawQuad>(),
                        append_quads_data);
      break;
    }
    case VideoFrameExternalResources::IO_SURFACE: {
      if (!hardware_resource_) {
        DCHECK_EQ(frame_resources_.size(), 1u);
        if (frame_resources_.size() < 1u)
          break;
      }
      gfx::Size visible_size(visible_rect.width(), visible_rect.height());
      scoped_ptr<IOSurfaceDrawQuad> io_surface_quad =
          IOSurfaceDrawQuad::Create();
      io_surface_quad->SetNew(shared_quad_state,
                              quad_rect,
                              opaque_rect,
                              visible_size,
                              hardware_resource_ ? hardware_resource_
                                                 : frame_resources_[0],
                              IOSurfaceDrawQuad::UNFLIPPED);
      quad_sink->Append(io_surface_quad.PassAs<DrawQuad>(),
                        append_quads_data);
      break;
    }
#if defined(GOOGLE_TV)
    // This block and other blocks wrapped around #if defined(GOOGLE_TV) is not
    // maintained by the general compositor team. Please contact the following
    // people instead:
    //
    // wonsik@chromium.org
    // ycheo@chromium.org
    case VideoFrameExternalResources::HOLE: {
      DCHECK_EQ(frame_resources_.size(), 0u);
      scoped_ptr<SolidColorDrawQuad> solid_color_draw_quad =
          SolidColorDrawQuad::Create();

      // Create a solid color quad with transparent black and force no
      // blending / no anti-aliasing.
      solid_color_draw_quad->SetAll(
          shared_quad_state, quad_rect, quad_rect, quad_rect, false,
          SK_ColorTRANSPARENT, true);
      quad_sink->Append(solid_color_draw_quad.PassAs<DrawQuad>(),
                        append_quads_data);
      break;
    }
#endif
    case VideoFrameExternalResources::NONE:
      NOTIMPLEMENTED();
      break;
  }
}

void VideoLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  if (!frame_)
    return;

  if (frame_resource_type_ ==
      VideoFrameExternalResources::SOFTWARE_RESOURCE) {
    for (size_t i = 0; i < software_resources_.size(); ++i)
      software_release_callback_.Run(0, false);

    software_resources_.clear();
    software_release_callback_.Reset();
  } else if (hardware_resource_) {
    hardware_release_callback_.Run(0, false);
    hardware_resource_ = 0;
    hardware_release_callback_.Reset();
  } else {
    for (size_t i = 0; i < frame_resources_.size(); ++i)
      resource_provider->DeleteResource(frame_resources_[i]);
    frame_resources_.clear();
  }

  provider_client_impl_->PutCurrentFrame(frame_);
  frame_ = NULL;

  provider_client_impl_->ReleaseLock();
}

void VideoLayerImpl::DidLoseOutputSurface() {
  updater_.reset();
}

void VideoLayerImpl::SetNeedsRedraw() {
  set_update_rect(gfx::UnionRects(update_rect(), gfx::RectF(bounds())));
  layer_tree_impl()->SetNeedsRedraw();
}

void VideoLayerImpl::SetProviderClientImpl(
    scoped_refptr<VideoFrameProviderClientImpl> provider_client_impl) {
  provider_client_impl_ = provider_client_impl;
}

const char* VideoLayerImpl::LayerTypeAsString() const {
  return "VideoLayer";
}

}  // namespace cc
