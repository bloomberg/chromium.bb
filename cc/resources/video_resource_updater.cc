// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include "base/bind.h"
#include "cc/output/gl_renderer.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/video_frame.h"
#include "media/filters/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/size_conversions.h"

const unsigned kYUVResourceFormat = GL_LUMINANCE;
const unsigned kRGBResourceFormat = GL_RGBA;

namespace cc {

VideoFrameExternalResources::VideoFrameExternalResources() : type(NONE) {}

VideoFrameExternalResources::~VideoFrameExternalResources() {}

VideoResourceUpdater::VideoResourceUpdater(ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
}

VideoResourceUpdater::~VideoResourceUpdater() {}

bool VideoResourceUpdater::VerifyFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  // If these fail, we'll have to add logic that handles offset bitmap/texture
  // UVs. For now, just expect (0, 0) offset, since all our decoders so far
  // don't offset.
  DCHECK_EQ(video_frame->visible_rect().x(), 0);
  DCHECK_EQ(video_frame->visible_rect().y(), 0);

  switch (video_frame->format()) {
    // Acceptable inputs.
    case media::VideoFrame::YV12:
    case media::VideoFrame::YV16:
    case media::VideoFrame::NATIVE_TEXTURE:
#if defined(GOOGLE_TV)
    case media::VideoFrame::HOLE:
#endif
      return true;

    // Unacceptable inputs. ¯\(°_o)/¯
    case media::VideoFrame::INVALID:
    case media::VideoFrame::RGB32:
    case media::VideoFrame::EMPTY:
    case media::VideoFrame::I420:
      break;
  }
  return false;
}

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
static gfx::Size SoftwarePlaneDimension(
    media::VideoFrame::Format input_frame_format,
    gfx::Size coded_size,
    GLenum output_resource_format,
    int plane_index) {
  if (output_resource_format == kYUVResourceFormat) {
    if (plane_index == media::VideoFrame::kYPlane)
      return coded_size;

    switch (input_frame_format) {
      case media::VideoFrame::YV12:
        return gfx::ToFlooredSize(gfx::ScaleSize(coded_size, 0.5f, 0.5f));
      case media::VideoFrame::YV16:
        return gfx::ToFlooredSize(gfx::ScaleSize(coded_size, 0.5f, 1.f));

      case media::VideoFrame::INVALID:
      case media::VideoFrame::RGB32:
      case media::VideoFrame::EMPTY:
      case media::VideoFrame::I420:
      case media::VideoFrame::NATIVE_TEXTURE:
#if defined(GOOGLE_TV)
      case media::VideoFrame::HOLE:
#endif
        NOTREACHED();
    }
  }

  DCHECK_EQ(output_resource_format, static_cast<unsigned>(kRGBResourceFormat));
  return coded_size;
}

static void ReleaseResource(ResourceProvider* resource_provider,
                            ResourceProvider::ResourceId resource_id,
                            unsigned sync_point) {
  resource_provider->DeleteResource(resource_id);
}

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  if (!VerifyFrame(video_frame))
    return VideoFrameExternalResources();

  media::VideoFrame::Format input_frame_format = video_frame->format();

#if defined(GOOGLE_TV)
  if (input_frame_format == media::VideoFrame::HOLE) {
    VideoFrameExternalResources external_resources;
    external_resources.type = VideoFrameExternalResources::HOLE;
    return external_resources;
  }
#endif

  // Only YUV software video frames are supported.
  DCHECK(input_frame_format == media::VideoFrame::YV12 ||
         input_frame_format == media::VideoFrame::YV16);
  if (input_frame_format != media::VideoFrame::YV12 &&
      input_frame_format != media::VideoFrame::YV16)
    return VideoFrameExternalResources();

  bool software_compositor = !resource_provider_->GraphicsContext3D();

  GLenum output_resource_format = kYUVResourceFormat;
  size_t output_plane_count = 3;

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor) {
    output_resource_format = kRGBResourceFormat;
    output_plane_count = 1;
  }

  int max_resource_size = resource_provider_->max_texture_size();
  gfx::Size coded_frame_size = video_frame->coded_size();

  ResourceProvider::ResourceIdArray plane_resources;
  bool allocation_success = true;

  for (size_t i = 0; i < output_plane_count; ++i) {
    gfx::Size plane_size =
        SoftwarePlaneDimension(input_frame_format,
                               coded_frame_size,
                               output_resource_format,
                               i);
    if (plane_size.IsEmpty() ||
        plane_size.width() > max_resource_size ||
        plane_size.height() > max_resource_size) {
      allocation_success = false;
      break;
    }

    // TODO(danakj): Could recycle resources that we previously allocated and
    // were returned to us.
    ResourceProvider::ResourceId resource_id =
        resource_provider_->CreateResource(plane_size,
                                           output_resource_format,
                                           ResourceProvider::TextureUsageAny);
    if (resource_id == 0) {
      allocation_success = false;
      break;
    }

    plane_resources.push_back(resource_id);
  }

  if (!allocation_success) {
    for (size_t i = 0; i < plane_resources.size(); ++i)
      resource_provider_->DeleteResource(plane_resources[i]);
    return VideoFrameExternalResources();
  }

  VideoFrameExternalResources external_resources;

  if (software_compositor) {
    DCHECK_EQ(output_resource_format, kRGBResourceFormat);
    DCHECK_EQ(plane_resources.size(), 1u);

    if (!video_renderer_)
      video_renderer_.reset(new media::SkCanvasVideoRenderer);

    {
      ResourceProvider::ScopedWriteLockSoftware lock(
          resource_provider_, plane_resources[0]);
      video_renderer_->Paint(video_frame,
                             lock.sk_canvas(),
                             video_frame->visible_rect(),
                             0xff);
    }

    // In software mode, the resource provider won't be lost. Soon this callback
    // will be called directly from the resource provider, same as 3d
    // compositing mode, so this raw unretained resource_provider will always
    // be valid when the callback is fired.
    TextureMailbox::ReleaseCallback callback_to_free_resource =
        base::Bind(&ReleaseResource,
                   base::Unretained(resource_provider_),
                   plane_resources[0]);
    external_resources.software_resources.push_back(plane_resources[0]);
    external_resources.software_release_callback = callback_to_free_resource;

    external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;
    return external_resources;
  }

  DCHECK_EQ(output_resource_format,
            static_cast<unsigned>(kYUVResourceFormat));

  WebKit::WebGraphicsContext3D* context =
      resource_provider_->GraphicsContext3D();
  DCHECK(context);

  for (size_t plane = 0; plane < plane_resources.size(); ++plane) {
    // Update each plane's resource id with its content.
    ResourceProvider::ResourceId output_plane_resource_id =
        plane_resources[plane];
    gfx::Size plane_size =
        SoftwarePlaneDimension(input_frame_format,
                               coded_frame_size,
                               output_resource_format,
                               plane);
    const uint8_t* input_plane_pixels = video_frame->data(plane);

    gfx::Rect image_rect(
        0, 0, video_frame->stride(plane), plane_size.height());
    gfx::Rect source_rect(plane_size);
    resource_provider_->SetPixels(output_plane_resource_id,
                                  input_plane_pixels,
                                  image_rect,
                                  source_rect,
                                  gfx::Vector2d());

    gpu::Mailbox mailbox;
    {
      ResourceProvider::ScopedWriteLockGL lock(
          resource_provider_, output_plane_resource_id);

      GLC(context, context->genMailboxCHROMIUM(mailbox.name));
      GLC(context, context->bindTexture(GL_TEXTURE_2D, lock.texture_id()));
      GLC(context, context->produceTextureCHROMIUM(GL_TEXTURE_2D,
                                                   mailbox.name));
      GLC(context, context->bindTexture(GL_TEXTURE_2D, 0));
    }

    // This callback is called by the resource provider itself, so it's okay to
    // use an unretained raw pointer here.
    TextureMailbox::ReleaseCallback callback_to_free_resource =
        base::Bind(&ReleaseResource,
                   base::Unretained(resource_provider_),
                   output_plane_resource_id);
    external_resources.mailboxes.push_back(
        TextureMailbox(mailbox, callback_to_free_resource));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame,
    const TextureMailbox::ReleaseCallback& release_callback) {
  if (!VerifyFrame(video_frame))
    return VideoFrameExternalResources();

  media::VideoFrame::Format frame_format = video_frame->format();

  DCHECK_EQ(frame_format, media::VideoFrame::NATIVE_TEXTURE);
  if (frame_format != media::VideoFrame::NATIVE_TEXTURE)
      return VideoFrameExternalResources();

  WebKit::WebGraphicsContext3D* context =
      resource_provider_->GraphicsContext3D();
  if (!context)
    return VideoFrameExternalResources();

  VideoFrameExternalResources external_resources;
  switch (video_frame->texture_target()) {
    case GL_TEXTURE_2D:
      external_resources.type = VideoFrameExternalResources::RGB_RESOURCE;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      external_resources.type =
          VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      external_resources.type = VideoFrameExternalResources::IO_SURFACE;
      break;
    default:
      NOTREACHED();
      return VideoFrameExternalResources();
  }

  gpu::Mailbox mailbox;
  GLC(context, context->genMailboxCHROMIUM(mailbox.name));
  GLC(context, context->bindTexture(GL_TEXTURE_2D, video_frame->texture_id()));
  GLC(context, context->produceTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name));
  GLC(context, context->bindTexture(GL_TEXTURE_2D, 0));

  TextureMailbox::ReleaseCallback callback_to_return_resource =
      base::Bind(&ReturnTexture,
                 base::Unretained(resource_provider_),
                 release_callback,
                 video_frame->texture_id(),
                 mailbox);
  external_resources.mailboxes.push_back(
      TextureMailbox(mailbox, callback_to_return_resource));
  return external_resources;
}

// static
void VideoResourceUpdater::ReturnTexture(
    ResourceProvider* resource_provider,
    TextureMailbox::ReleaseCallback callback,
    unsigned texture_id,
    gpu::Mailbox mailbox,
    unsigned sync_point) {
  WebKit::WebGraphicsContext3D* context =
      resource_provider->GraphicsContext3D();
  GLC(context, context->bindTexture(GL_TEXTURE_2D, texture_id));
  GLC(context, context->consumeTextureCHROMIUM(GL_TEXTURE_2D, mailbox.name));
  GLC(context, context->bindTexture(GL_TEXTURE_2D, 0));
  callback.Run(sync_point);
}

}  // namespace cc
