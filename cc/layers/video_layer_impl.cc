// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/video_layer_impl.h"

#include "base/logging.h"
#include "cc/base/math_util.h"
#include "cc/layers/quad_sink.h"
#include "cc/layers/video_frame_provider_client_impl.h"
#include "cc/output/renderer.h"
#include "cc/quads/io_surface_draw_quad.h"
#include "cc/quads/stream_video_draw_quad.h"
#include "cc/quads/texture_draw_quad.h"
#include "cc/quads/yuv_video_draw_quad.h"
#include "cc/resources/resource_provider.h"
#include "cc/trees/layer_tree_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"

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
      format_(media::VideoFrame::INVALID),
      convert_yuv_(false),
      external_texture_resource_(0) {}

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
  FreeFramePlanes(layer_tree_impl()->resource_provider());

#ifndef NDEBUG
  for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; ++i)
    DCHECK(!frame_planes_[i].resource_id);
  DCHECK(!external_texture_resource_);
#endif
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

  WillDrawInternal(resource_provider);
  FreeUnusedFramePlanes(resource_provider);

  if (!frame_)
    provider_client_impl_->ReleaseLock();
}

void VideoLayerImpl::WillDrawInternal(ResourceProvider* resource_provider) {
  DCHECK(!external_texture_resource_);

  if (!frame_)
    return;

  format_ = frame_->format();

#if defined(GOOGLE_TV)
  if (format_ == media::VideoFrame::HOLE)
    return;
#endif

  // If these fail, we'll have to add draw logic that handles offset bitmap/
  // texture UVs.  For now, just expect (0, 0) offset, since all our decoders
  // so far don't offset.
  DCHECK_EQ(frame_->visible_rect().x(), 0);
  DCHECK_EQ(frame_->visible_rect().y(), 0);

  if (format_ == media::VideoFrame::INVALID) {
    provider_client_impl_->PutCurrentFrame(frame_);
    frame_ = NULL;
    return;
  }

  // TODO: If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  convert_yuv_ =
      resource_provider->default_resource_type() == ResourceProvider::Bitmap &&
      (format_ == media::VideoFrame::YV12 ||
       format_ == media::VideoFrame::YV16);

  if (convert_yuv_)
    format_ = media::VideoFrame::RGB32;

  if (!SetupFramePlanes(resource_provider)) {
    provider_client_impl_->PutCurrentFrame(frame_);
    frame_ = NULL;
    return;
  }

  if (format_ == media::VideoFrame::NATIVE_TEXTURE &&
      frame_->texture_target() == GL_TEXTURE_2D) {
    external_texture_resource_ =
        resource_provider->CreateResourceFromExternalTexture(
            frame_->texture_id());
  }
}

void VideoLayerImpl::AppendQuads(QuadSink* quad_sink,
                                 AppendQuadsData* append_quads_data) {
  if (!frame_)
    return;

  SharedQuadState* shared_quad_state =
      quad_sink->UseSharedQuadState(CreateSharedQuadState());
  AppendDebugBorderQuad(quad_sink, shared_quad_state, append_quads_data);

  // TODO: When we pass quads out of process, we need to double-buffer, or
  // otherwise synchonize use of all textures in the quad.

  gfx::Rect quad_rect(content_bounds());
  gfx::Rect opaque_rect(contents_opaque() ? quad_rect : gfx::Rect());
  gfx::Rect visible_rect = frame_->visible_rect();
  gfx::Size coded_size = frame_->coded_size();

  // pixels for macroblocked formats.
  float tex_width_scale =
      static_cast<float>(visible_rect.width()) / coded_size.width();
  float tex_height_scale =
      static_cast<float>(visible_rect.height()) / coded_size.height();

#if defined(GOOGLE_TV)
  // This block and other blocks wrapped around #if defined(GOOGLE_TV) is not
  // maintained by the general compositor team. Please contact the following
  // people instead:
  //
  // wonsik@chromium.org
  // ycheo@chromium.org

  if (frame_->format() == media::VideoFrame::HOLE) {
    scoped_ptr<SolidColorDrawQuad> solid_color_draw_quad =
        SolidColorDrawQuad::Create();
    // Create a solid color quad with transparent black and force no
    // blending.
    solid_color_draw_quad->SetAll(
        shared_quad_state, quad_rect, quad_rect, quad_rect, false,
        SK_ColorTRANSPARENT);
    quad_sink->Append(solid_color_draw_quad.PassAs<DrawQuad>(),
                      append_quads_data);
    return;
  }
#endif

  switch (format_) {
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16: {
      // YUV software decoder.
      const FramePlane& y_plane = frame_planes_[media::VideoFrame::kYPlane];
      const FramePlane& u_plane = frame_planes_[media::VideoFrame::kUPlane];
      const FramePlane& v_plane = frame_planes_[media::VideoFrame::kVPlane];
      gfx::SizeF tex_scale(tex_width_scale, tex_height_scale);
      scoped_ptr<YUVVideoDrawQuad> yuv_video_quad = YUVVideoDrawQuad::Create();
      yuv_video_quad->SetNew(shared_quad_state,
                             quad_rect,
                             opaque_rect,
                             tex_scale,
                             y_plane,
                             u_plane,
                             v_plane);
      quad_sink->Append(yuv_video_quad.PassAs<DrawQuad>(), append_quads_data);
      break;
    }
    case media::VideoFrame::RGB32: {
      // RGBA software decoder: a converted YUV frame (see: convert_yuv_).
      const FramePlane& plane = frame_planes_[media::VideoFrame::kRGBPlane];
      bool premultiplied_alpha = true;
      gfx::PointF uv_top_left(0.f, 0.f);
      gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
      float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
      bool flipped = false;
      scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
      texture_quad->SetNew(shared_quad_state,
                           quad_rect,
                           opaque_rect,
                           plane.resource_id,
                           premultiplied_alpha,
                           uv_top_left,
                           uv_bottom_right,
                           opacity,
                           flipped);
      quad_sink->Append(texture_quad.PassAs<DrawQuad>(), append_quads_data);
      break;
    }
    case media::VideoFrame::NATIVE_TEXTURE:
      switch (frame_->texture_target()) {
        case GL_TEXTURE_2D: {
          // NativeTexture hardware decoder.
          bool premultiplied_alpha = true;
          gfx::PointF uv_top_left(0.f, 0.f);
          gfx::PointF uv_bottom_right(tex_width_scale, tex_height_scale);
          float opacity[] = {1.0f, 1.0f, 1.0f, 1.0f};
          bool flipped = false;
          scoped_ptr<TextureDrawQuad> texture_quad = TextureDrawQuad::Create();
          texture_quad->SetNew(shared_quad_state,
                               quad_rect,
                               opaque_rect,
                               external_texture_resource_,
                               premultiplied_alpha,
                               uv_top_left,
                               uv_bottom_right,
                               opacity,
                               flipped);
          quad_sink->Append(texture_quad.PassAs<DrawQuad>(), append_quads_data);
          break;
        }
        case GL_TEXTURE_RECTANGLE_ARB: {
          gfx::Size visible_size(visible_rect.width(), visible_rect.height());
          scoped_ptr<IOSurfaceDrawQuad> io_surface_quad =
              IOSurfaceDrawQuad::Create();
          io_surface_quad->SetNew(shared_quad_state,
                                  quad_rect,
                                  opaque_rect,
                                  visible_size,
                                  frame_->texture_id(),
                                  IOSurfaceDrawQuad::UNFLIPPED);
          quad_sink->Append(io_surface_quad.PassAs<DrawQuad>(),
                            append_quads_data);
          break;
        }
        case GL_TEXTURE_EXTERNAL_OES: {
          // StreamTexture hardware decoder.
          gfx::Transform transform(
              provider_client_impl_->stream_texture_matrix());
          transform.Scale(tex_width_scale, tex_height_scale);
          scoped_ptr<StreamVideoDrawQuad> stream_video_quad =
              StreamVideoDrawQuad::Create();
          stream_video_quad->SetNew(shared_quad_state,
                                    quad_rect,
                                    opaque_rect,
                                    frame_->texture_id(),
                                    transform);
          quad_sink->Append(stream_video_quad.PassAs<DrawQuad>(),
                            append_quads_data);
          break;
        }
        default:
          NOTREACHED();
          break;
      }
      break;
    case media::VideoFrame::INVALID:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
#if defined(GOOGLE_TV)
    case media::VideoFrame::HOLE:
#endif
      NOTREACHED();
      break;
  }
}

void VideoLayerImpl::DidDraw(ResourceProvider* resource_provider) {
  LayerImpl::DidDraw(resource_provider);

  if (!frame_)
    return;

  if (format_ == media::VideoFrame::NATIVE_TEXTURE &&
      frame_->texture_target() == GL_TEXTURE_2D) {
    DCHECK(external_texture_resource_);
    // TODO: the following assert will not be true when sending resources to a
    // parent compositor. We will probably need to hold on to frame_ for
    // longer, and have several "current frames" in the pipeline.
    DCHECK(!resource_provider->InUseByConsumer(external_texture_resource_));
    resource_provider->DeleteResource(external_texture_resource_);
    external_texture_resource_ = 0;
  }

  provider_client_impl_->PutCurrentFrame(frame_);
  frame_ = NULL;

  provider_client_impl_->ReleaseLock();
}

static gfx::Size VideoFrameDimension(media::VideoFrame* frame, int plane) {
  gfx::Size dimensions = frame->coded_size();
  switch (frame->format()) {
    case media::VideoFrame::YV12:
      if (plane != media::VideoFrame::kYPlane) {
        dimensions.set_width(dimensions.width() / 2);
        dimensions.set_height(dimensions.height() / 2);
      }
      break;
    case media::VideoFrame::YV16:
      if (plane != media::VideoFrame::kYPlane)
        dimensions.set_width(dimensions.width() / 2);
      break;
    default:
      break;
  }
  return dimensions;
}

bool VideoLayerImpl::FramePlane::AllocateData(
    ResourceProvider* resource_provider) {
  if (resource_id)
    return true;

  resource_id = resource_provider->CreateResource(
      size, format, ResourceProvider::TextureUsageAny);
  return resource_id;
}

void VideoLayerImpl::FramePlane::FreeData(ResourceProvider* resource_provider) {
  if (!resource_id)
    return;

  resource_provider->DeleteResource(resource_id);
  resource_id = 0;
}

// Convert media::VideoFrame::Format to OpenGL enum values.
static GLenum ConvertVFCFormatToGLenum(const media::VideoFrame::Format format) {
  switch (format) {
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16:
      return GL_LUMINANCE;
    case media::VideoFrame::RGB32:
      return GL_RGBA;
    case media::VideoFrame::NATIVE_TEXTURE:
#if defined(GOOGLE_TV)
    case media::VideoFrame::HOLE:
#endif
    case media::VideoFrame::INVALID:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
      NOTREACHED();
      break;
  }
  return GL_INVALID_VALUE;
}

bool VideoLayerImpl::SetupFramePlanes(ResourceProvider* resource_provider) {
  const size_t plane_count = media::VideoFrame::NumPlanes(format_);
  if (!plane_count)
    return true;

  const int max_texture_size = resource_provider->max_texture_size();
  const GLenum pixel_format = ConvertVFCFormatToGLenum(format_);
  for (size_t plane_index = 0; plane_index < plane_count; ++plane_index) {
    VideoLayerImpl::FramePlane* plane = &frame_planes_[plane_index];

    gfx::Size required_texture_size = VideoFrameDimension(frame_, plane_index);
    // TODO: Remove the test against max_texture_size when tiled layers are
    // implemented.
    if (required_texture_size.IsEmpty() ||
        required_texture_size.width() > max_texture_size ||
        required_texture_size.height() > max_texture_size)
      return false;

    if (plane->size != required_texture_size || plane->format != pixel_format) {
      plane->FreeData(resource_provider);
      plane->size = required_texture_size;
      plane->format = pixel_format;
    }

    if (!plane->AllocateData(resource_provider))
      return false;
  }

  if (convert_yuv_) {
    if (!video_renderer_)
      video_renderer_.reset(new media::SkCanvasVideoRenderer);
    const VideoLayerImpl::FramePlane& plane =
        frame_planes_[media::VideoFrame::kRGBPlane];
    ResourceProvider::ScopedWriteLockSoftware lock(resource_provider,
                                                   plane.resource_id);
    video_renderer_->Paint(frame_,
                           lock.sk_canvas(),
                           frame_->visible_rect(),
                           0xff);
    return true;
  }

  for (size_t plane_index = 0; plane_index < plane_count; ++plane_index) {
    const VideoLayerImpl::FramePlane& plane = frame_planes_[plane_index];
    // Only planar formats planes should need upload.
    DCHECK_EQ(plane.format, GL_LUMINANCE);
    const uint8_t* software_plane_pixels = frame_->data(plane_index);
    gfx::Rect image_rect(0,
                         0,
                         frame_->stride(plane_index),
                         plane.size.height());
    gfx::Rect source_rect(plane.size);
    resource_provider->SetPixels(plane.resource_id,
                                 software_plane_pixels,
                                 image_rect,
                                 source_rect,
                                 gfx::Vector2d());
  }
  return true;
}

void VideoLayerImpl::FreeFramePlanes(ResourceProvider* resource_provider) {
  for (size_t i = 0; i < media::VideoFrame::kMaxPlanes; ++i)
    frame_planes_[i].FreeData(resource_provider);
}

void VideoLayerImpl::FreeUnusedFramePlanes(ResourceProvider* resource_provider) {
  size_t first_unused_plane = (frame_ ? media::VideoFrame::NumPlanes(format_)
                                      : 0);
  for (size_t i = first_unused_plane; i < media::VideoFrame::kMaxPlanes; ++i)
    frame_planes_[i].FreeData(resource_provider);
}

void VideoLayerImpl::DidLoseOutputSurface() {
  FreeFramePlanes(layer_tree_impl()->resource_provider());
}

void VideoLayerImpl::SetNeedsRedraw() {
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
