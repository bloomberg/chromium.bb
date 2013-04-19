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

VideoFrameExternalResources::VideoFrameExternalResources()
    : type(NONE), hardware_resource(0) {}

VideoFrameExternalResources::~VideoFrameExternalResources() {}

VideoResourceUpdater::VideoResourceUpdater(ResourceProvider* resource_provider)
    : resource_provider_(resource_provider) {
}

VideoResourceUpdater::~VideoResourceUpdater() {
  while (!recycled_resources_.empty()) {
    resource_provider_->DeleteResource(recycled_resources_.back().resource_id);
    recycled_resources_.pop_back();
  }
}

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
    case media::VideoFrame::YV12A:
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
      case media::VideoFrame::YV12A:
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
         input_frame_format == media::VideoFrame::YV12A ||
         input_frame_format == media::VideoFrame::YV16);
  if (input_frame_format != media::VideoFrame::YV12 &&
      input_frame_format != media::VideoFrame::YV12A &&
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

  std::vector<PlaneResource> plane_resources;
  bool allocation_success = true;

  for (size_t i = 0; i < output_plane_count; ++i) {
    gfx::Size output_plane_resource_size =
        SoftwarePlaneDimension(input_frame_format,
                               coded_frame_size,
                               output_resource_format,
                               i);
    if (output_plane_resource_size.IsEmpty() ||
        output_plane_resource_size.width() > max_resource_size ||
        output_plane_resource_size.height() > max_resource_size) {
      allocation_success = false;
      break;
    }

    ResourceProvider::ResourceId resource_id = 0;
    unsigned sync_point = 0;

    // Try recycle a previously-allocated resource.
    for (size_t i = 0; i < recycled_resources_.size(); ++i) {
      if (recycled_resources_[i].resource_format == output_resource_format &&
          recycled_resources_[i].resource_size == output_plane_resource_size) {
        resource_id = recycled_resources_[i].resource_id;
        sync_point = recycled_resources_[i].sync_point;
        recycled_resources_.erase(recycled_resources_.begin() + i);
        break;
      }
    }

    if (resource_id == 0) {
      // TODO(danakj): Abstract out hw/sw resource create/delete from
      // ResourceProvider and stop using ResourceProvider in this class.
      resource_id =
          resource_provider_->CreateResource(output_plane_resource_size,
                                             output_resource_format,
                                             ResourceProvider::TextureUsageAny);
    }

    if (resource_id == 0) {
      allocation_success = false;
      break;
    }

    plane_resources.push_back(PlaneResource(resource_id,
                                            output_plane_resource_size,
                                            output_resource_format,
                                            sync_point));
  }

  if (!allocation_success) {
    for (size_t i = 0; i < plane_resources.size(); ++i)
      resource_provider_->DeleteResource(plane_resources[i].resource_id);
    return VideoFrameExternalResources();
  }

  VideoFrameExternalResources external_resources;

  if (software_compositor) {
    DCHECK_EQ(plane_resources.size(), 1u);
    DCHECK_EQ(plane_resources[0].resource_format, kRGBResourceFormat);
    DCHECK_EQ(plane_resources[0].sync_point, 0u);

    if (!video_renderer_)
      video_renderer_.reset(new media::SkCanvasVideoRenderer);

    {
      ResourceProvider::ScopedWriteLockSoftware lock(
          resource_provider_, plane_resources[0].resource_id);
      video_renderer_->Paint(video_frame,
                             lock.sk_canvas(),
                             video_frame->visible_rect(),
                             0xff);
    }

    // In software mode, the resource provider won't be lost. Soon this callback
    // will be called directly from the resource provider, same as 3d
    // compositing mode, so this raw unretained resource_provider will always
    // be valid when the callback is fired.
    RecycleResourceData recycle_data = {
      plane_resources[0].resource_id,
      plane_resources[0].resource_size,
      plane_resources[0].resource_format,
    };
    TextureMailbox::ReleaseCallback callback_to_free_resource =
        base::Bind(&RecycleResource,
                   AsWeakPtr(),
                   base::Unretained(resource_provider_),
                   recycle_data);
    external_resources.software_resources.push_back(
        plane_resources[0].resource_id);
    external_resources.software_release_callback = callback_to_free_resource;

    external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;
    return external_resources;
  }

  WebKit::WebGraphicsContext3D* context =
      resource_provider_->GraphicsContext3D();
  DCHECK(context);

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    // Update each plane's resource id with its content.
    DCHECK_EQ(plane_resources[i].resource_format,
              static_cast<unsigned>(kYUVResourceFormat));

    const uint8_t* input_plane_pixels = video_frame->data(i);

    gfx::Rect image_rect(0,
                         0,
                         video_frame->stride(i),
                         plane_resources[i].resource_size.height());
    gfx::Rect source_rect(plane_resources[i].resource_size);
    resource_provider_->SetPixels(plane_resources[i].resource_id,
                                  input_plane_pixels,
                                  image_rect,
                                  source_rect,
                                  gfx::Vector2d());

    gpu::Mailbox mailbox;
    {
      ResourceProvider::ScopedWriteLockGL lock(
          resource_provider_, plane_resources[i].resource_id);

      GLC(context, context->genMailboxCHROMIUM(mailbox.name));
      GLC(context, context->bindTexture(GL_TEXTURE_2D, lock.texture_id()));
      GLC(context, context->produceTextureCHROMIUM(GL_TEXTURE_2D,
                                                   mailbox.name));
      GLC(context, context->bindTexture(GL_TEXTURE_2D, 0));
    }

    // This callback is called by the resource provider itself, so it's okay to
    // use an unretained raw pointer here.
    RecycleResourceData recycle_data = {
      plane_resources[i].resource_id,
      plane_resources[i].resource_size,
      plane_resources[i].resource_format,
      mailbox
    };
    TextureMailbox::ReleaseCallback callback_to_free_resource =
        base::Bind(&RecycleResource,
                   AsWeakPtr(),
                   base::Unretained(resource_provider_),
                   recycle_data);
    external_resources.mailboxes.push_back(
        TextureMailbox(mailbox,
                       callback_to_free_resource,
                       plane_resources[i].sync_point));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
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

  external_resources.hardware_resource =
      resource_provider_->CreateResourceFromExternalTexture(
          video_frame->texture_target(),
          video_frame->texture_id());

  TextureMailbox::ReleaseCallback callback_to_return_resource =
      base::Bind(&ReturnTexture,
                 base::Unretained(resource_provider_),
                 external_resources.hardware_resource);
  external_resources.hardware_release_callback = callback_to_return_resource;
  return external_resources;
}

// static
void VideoResourceUpdater::ReturnTexture(
    ResourceProvider* resource_provider,
    unsigned resource_id,
    unsigned sync_point,
    bool lost_resource) {
  resource_provider->DeleteResource(resource_id);
}

// static
void VideoResourceUpdater::RecycleResource(
    base::WeakPtr<VideoResourceUpdater> updater,
    ResourceProvider* resource_provider,
    RecycleResourceData data,
    unsigned sync_point,
    bool lost_resource) {
  WebKit::WebGraphicsContext3D* context =
      resource_provider->GraphicsContext3D();
  if (context && sync_point)
      GLC(context, context->waitSyncPoint(sync_point));
  if (context && !lost_resource) {
    ResourceProvider::ScopedWriteLockGL lock(resource_provider,
                                             data.resource_id);
    GLC(context, context->bindTexture(GL_TEXTURE_2D, lock.texture_id()));
    GLC(context, context->consumeTextureCHROMIUM(GL_TEXTURE_2D,
                                                 data.mailbox.name));
    GLC(context, context->bindTexture(GL_TEXTURE_2D, 0));
  }

  if (!updater || lost_resource) {
    resource_provider->DeleteResource(data.resource_id);
    return;
  }

  // Drop recycled resources that are the wrong format.
  while (!updater->recycled_resources_.empty() &&
         updater->recycled_resources_.back().resource_format !=
         data.resource_format) {
    resource_provider->DeleteResource(
        updater->recycled_resources_.back().resource_id);
    updater->recycled_resources_.pop_back();
  }

  PlaneResource recycled_resource(data.resource_id,
                                  data.resource_size,
                                  data.resource_format,
                                  sync_point);
  updater->recycled_resources_.push_back(recycled_resource);
}

}  // namespace cc
