// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/video_resource_updater.h"

#include <algorithm>

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/util.h"
#include "cc/output/gl_renderer.h"
#include "cc/resources/resource_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/blink/skcanvas_video_renderer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

namespace {

const ResourceFormat kRGBResourceFormat = RGBA_8888;

class SyncPointClientImpl : public media::VideoFrame::SyncPointClient {
 public:
  explicit SyncPointClientImpl(gpu::gles2::GLES2Interface* gl) : gl_(gl) {}
  ~SyncPointClientImpl() override {}
  uint32 InsertSyncPoint() override {
    return GLC(gl_, gl_->InsertSyncPointCHROMIUM());
  }
  void WaitSyncPoint(uint32 sync_point) override {
    GLC(gl_, gl_->WaitSyncPointCHROMIUM(sync_point));
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
};

}  // namespace

VideoResourceUpdater::PlaneResource::PlaneResource(
    unsigned int resource_id,
    const gfx::Size& resource_size,
    ResourceFormat resource_format,
    gpu::Mailbox mailbox)
    : resource_id(resource_id),
      resource_size(resource_size),
      resource_format(resource_format),
      mailbox(mailbox),
      ref_count(0),
      frame_ptr(nullptr),
      plane_index(0) {
}

bool VideoResourceUpdater::PlaneResourceMatchesUniqueID(
    const PlaneResource& plane_resource,
    const media::VideoFrame* video_frame,
    int plane_index) {
  return plane_resource.frame_ptr == video_frame &&
         plane_resource.plane_index == plane_index &&
         plane_resource.timestamp == video_frame->timestamp();
}

void VideoResourceUpdater::SetPlaneResourceUniqueId(
    const media::VideoFrame* video_frame,
    int plane_index,
    PlaneResource* plane_resource) {
  plane_resource->frame_ptr = video_frame;
  plane_resource->plane_index = plane_index;
  plane_resource->timestamp = video_frame->timestamp();
}

VideoFrameExternalResources::VideoFrameExternalResources() : type(NONE) {}

VideoFrameExternalResources::~VideoFrameExternalResources() {}

VideoResourceUpdater::VideoResourceUpdater(ContextProvider* context_provider,
                                           ResourceProvider* resource_provider)
    : context_provider_(context_provider),
      resource_provider_(resource_provider) {
}

VideoResourceUpdater::~VideoResourceUpdater() {
  for (const PlaneResource& plane_resource : all_resources_)
    resource_provider_->DeleteResource(plane_resource.resource_id);
}

VideoResourceUpdater::ResourceList::iterator
VideoResourceUpdater::AllocateResource(const gfx::Size& plane_size,
                                       ResourceFormat format,
                                       bool has_mailbox) {
  // TODO(danakj): Abstract out hw/sw resource create/delete from
  // ResourceProvider and stop using ResourceProvider in this class.
  const ResourceProvider::ResourceId resource_id =
      resource_provider_->CreateResource(
          plane_size, GL_CLAMP_TO_EDGE,
          ResourceProvider::TEXTURE_HINT_IMMUTABLE, format);
  if (resource_id == 0)
    return all_resources_.end();

  gpu::Mailbox mailbox;
  if (has_mailbox) {
    DCHECK(context_provider_);

    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    GLC(gl, gl->GenMailboxCHROMIUM(mailbox.name));
    ResourceProvider::ScopedWriteLockGL lock(resource_provider_, resource_id);
    GLC(gl, gl->ProduceTextureDirectCHROMIUM(lock.texture_id(), GL_TEXTURE_2D,
                                             mailbox.name));
  }
  all_resources_.push_front(
      PlaneResource(resource_id, plane_size, format, mailbox));
  return all_resources_.begin();
}

void VideoResourceUpdater::DeleteResource(ResourceList::iterator resource_it) {
  DCHECK_EQ(resource_it->ref_count, 0);
  resource_provider_->DeleteResource(resource_it->resource_id);
  all_resources_.erase(resource_it);
}

VideoFrameExternalResources VideoResourceUpdater::
    CreateExternalResourcesFromVideoFrame(
        const scoped_refptr<media::VideoFrame>& video_frame) {
  if (!VerifyFrame(video_frame))
    return VideoFrameExternalResources();

  if (video_frame->format() == media::VideoFrame::NATIVE_TEXTURE)
    return CreateForHardwarePlanes(video_frame);
  else
    return CreateForSoftwarePlanes(video_frame);
}

bool VideoResourceUpdater::VerifyFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  switch (video_frame->format()) {
    // Acceptable inputs.
    case media::VideoFrame::YV12:
    case media::VideoFrame::I420:
    case media::VideoFrame::YV12A:
    case media::VideoFrame::YV16:
    case media::VideoFrame::YV12J:
    case media::VideoFrame::YV12HD:
    case media::VideoFrame::YV24:
    case media::VideoFrame::NATIVE_TEXTURE:
#if defined(VIDEO_HOLE)
    case media::VideoFrame::HOLE:
#endif  // defined(VIDEO_HOLE)
    case media::VideoFrame::ARGB:
      return true;

    // Unacceptable inputs. ¯\(°_o)/¯
    case media::VideoFrame::UNKNOWN:
    case media::VideoFrame::NV12:
      break;
  }
  return false;
}

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
static gfx::Size SoftwarePlaneDimension(
    const scoped_refptr<media::VideoFrame>& input_frame,
    bool software_compositor,
    size_t plane_index) {
  if (!software_compositor) {
    return media::VideoFrame::PlaneSize(
        input_frame->format(), plane_index, input_frame->coded_size());
  }
  return input_frame->coded_size();
}

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForSoftwarePlanes");
  media::VideoFrame::Format input_frame_format = video_frame->format();

#if defined(VIDEO_HOLE)
  if (input_frame_format == media::VideoFrame::HOLE) {
    VideoFrameExternalResources external_resources;
    external_resources.type = VideoFrameExternalResources::HOLE;
    return external_resources;
  }
#endif  // defined(VIDEO_HOLE)

  // Only YUV software video frames are supported.
  if (input_frame_format != media::VideoFrame::YV12 &&
      input_frame_format != media::VideoFrame::I420 &&
      input_frame_format != media::VideoFrame::YV12A &&
      input_frame_format != media::VideoFrame::YV12J &&
      input_frame_format != media::VideoFrame::YV12HD &&
      input_frame_format != media::VideoFrame::YV16 &&
      input_frame_format != media::VideoFrame::YV24) {
    NOTREACHED() << input_frame_format;
    return VideoFrameExternalResources();
  }

  bool software_compositor = context_provider_ == NULL;

  ResourceFormat output_resource_format =
      resource_provider_->yuv_resource_format();
  size_t output_plane_count = media::VideoFrame::NumPlanes(input_frame_format);

  // TODO(skaslev): If we're in software compositing mode, we do the YUV -> RGB
  // conversion here. That involves an extra copy of each frame to a bitmap.
  // Obviously, this is suboptimal and should be addressed once ubercompositor
  // starts shaping up.
  if (software_compositor) {
    output_resource_format = kRGBResourceFormat;
    output_plane_count = 1;
  }

  // Drop recycled resources that are the wrong format.
  for (auto it = all_resources_.begin(); it != all_resources_.end();) {
    if (it->ref_count == 0 && it->resource_format != output_resource_format)
      DeleteResource(it++);
    else
      ++it;
  }

  const int max_resource_size = resource_provider_->max_texture_size();
  std::vector<ResourceList::iterator> plane_resources;
  for (size_t i = 0; i < output_plane_count; ++i) {
    gfx::Size output_plane_resource_size =
        SoftwarePlaneDimension(video_frame, software_compositor, i);
    if (output_plane_resource_size.IsEmpty() ||
        output_plane_resource_size.width() > max_resource_size ||
        output_plane_resource_size.height() > max_resource_size) {
      break;
    }

    // Try recycle a previously-allocated resource.
    ResourceList::iterator resource_it = all_resources_.end();
    for (auto it = all_resources_.begin(); it != all_resources_.end(); ++it) {
      if (it->resource_size == output_plane_resource_size &&
          it->resource_format == output_resource_format) {
        if (PlaneResourceMatchesUniqueID(*it, video_frame.get(), i)) {
          // Bingo, we found a resource that already contains the data we are
          // planning to put in it. It's safe to reuse it even if
          // resource_provider_ holds some references to it, because those
          // references are read-only.
          resource_it = it;
          break;
        }

        // This extra check is needed because resources backed by SharedMemory
        // are not ref-counted, unlike mailboxes. Full discussion in
        // codereview.chromium.org/145273021.
        const bool in_use =
            software_compositor &&
            resource_provider_->InUseByConsumer(it->resource_id);
        if (it->ref_count == 0 && !in_use) {
          // We found a resource with the correct size that we can overwrite.
          resource_it = it;
        }
      }
    }

    // Check if we need to allocate a new resource.
    if (resource_it == all_resources_.end()) {
      resource_it =
          AllocateResource(output_plane_resource_size, output_resource_format,
                           !software_compositor);
    }
    if (resource_it == all_resources_.end())
      break;

    ++resource_it->ref_count;
    plane_resources.push_back(resource_it);
  }

  if (plane_resources.size() != output_plane_count) {
    // Allocation failed, nothing will be returned so restore reference counts.
    for (ResourceList::iterator resource_it : plane_resources)
      --resource_it->ref_count;
    return VideoFrameExternalResources();
  }

  VideoFrameExternalResources external_resources;

  if (software_compositor) {
    DCHECK_EQ(plane_resources.size(), 1u);
    PlaneResource& plane_resource = *plane_resources[0];
    DCHECK_EQ(plane_resource.resource_format, kRGBResourceFormat);
    DCHECK(plane_resource.mailbox.IsZero());

    if (!PlaneResourceMatchesUniqueID(plane_resource, video_frame.get(), 0)) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (!video_renderer_)
        video_renderer_.reset(new media::SkCanvasVideoRenderer);

      ResourceProvider::ScopedWriteLockSoftware lock(
          resource_provider_, plane_resource.resource_id);
      SkCanvas canvas(lock.sk_bitmap());
      // This is software path, so canvas and video_frame are always backed
      // by software.
      video_renderer_->Copy(video_frame, &canvas, media::Context3D());
      SetPlaneResourceUniqueId(video_frame.get(), 0, &plane_resource);
    }

    external_resources.software_resources.push_back(plane_resource.resource_id);
    external_resources.software_release_callback =
        base::Bind(&RecycleResource, AsWeakPtr(), plane_resource.resource_id);
    external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;
    return external_resources;
  }

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    PlaneResource& plane_resource = *plane_resources[i];
    // Update each plane's resource id with its content.
    DCHECK_EQ(plane_resource.resource_format,
              resource_provider_->yuv_resource_format());

    if (!PlaneResourceMatchesUniqueID(plane_resource, video_frame.get(), i)) {
      // We need to transfer data from |video_frame| to the plane resource.
      // TODO(reveman): Can use GpuMemoryBuffers here to improve performance.

      // The |resource_size_pixels| is the size of the resource we want to
      // upload to.
      gfx::Size resource_size_pixels = plane_resource.resource_size;
      // The |video_stride_pixels| is the width of the video frame we are
      // uploading (including non-frame data to fill in the stride).
      size_t video_stride_pixels = video_frame->stride(i);

      size_t bytes_per_pixel = BitsPerPixel(plane_resource.resource_format) / 8;
      // Use 4-byte row alignment (OpenGL default) for upload performance.
      // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
      size_t upload_image_stride =
          RoundUp<size_t>(bytes_per_pixel * resource_size_pixels.width(), 4u);

      const uint8_t* pixels;
      if (upload_image_stride == video_stride_pixels * bytes_per_pixel) {
        pixels = video_frame->data(i);
      } else {
        // Avoid malloc for each frame/plane if possible.
        size_t needed_size =
            upload_image_stride * resource_size_pixels.height();
        if (upload_pixels_.size() < needed_size)
          upload_pixels_.resize(needed_size);
        for (int row = 0; row < resource_size_pixels.height(); ++row) {
          uint8_t* dst = &upload_pixels_[upload_image_stride * row];
          const uint8_t* src = video_frame->data(i) +
                               bytes_per_pixel * video_stride_pixels * row;
          memcpy(dst, src, resource_size_pixels.width() * bytes_per_pixel);
        }
        pixels = &upload_pixels_[0];
      }

      resource_provider_->CopyToResource(plane_resource.resource_id, pixels,
                                         resource_size_pixels);
      SetPlaneResourceUniqueId(video_frame.get(), i, &plane_resource);
    }

    external_resources.mailboxes.push_back(
        TextureMailbox(plane_resource.mailbox, GL_TEXTURE_2D, 0));
    external_resources.release_callbacks.push_back(
        base::Bind(&RecycleResource, AsWeakPtr(), plane_resource.resource_id));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

// static
void VideoResourceUpdater::ReturnTexture(
    base::WeakPtr<VideoResourceUpdater> updater,
    const scoped_refptr<media::VideoFrame>& video_frame,
    uint32 sync_point,
    bool lost_resource,
    BlockingTaskRunner* main_thread_task_runner) {
  // TODO(dshwang) this case should be forwarded to the decoder as lost
  // resource.
  if (lost_resource || !updater.get())
    return;
  // VideoFrame::UpdateReleaseSyncPoint() creates new sync point using the same
  // GL context which created the given |sync_point|, so discard the
  // |sync_point|.
  SyncPointClientImpl client(updater->context_provider_->ContextGL());
  video_frame->UpdateReleaseSyncPoint(&client);
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForHardwarePlanes");
  media::VideoFrame::Format frame_format = video_frame->format();

  DCHECK_EQ(frame_format, media::VideoFrame::NATIVE_TEXTURE);
  if (frame_format != media::VideoFrame::NATIVE_TEXTURE)
      return VideoFrameExternalResources();

  if (!context_provider_)
    return VideoFrameExternalResources();

  const gpu::MailboxHolder* mailbox_holder = video_frame->mailbox_holder();
  VideoFrameExternalResources external_resources;
  switch (mailbox_holder->texture_target) {
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

  external_resources.mailboxes.push_back(
      TextureMailbox(mailbox_holder->mailbox,
                     mailbox_holder->texture_target,
                     mailbox_holder->sync_point));
  external_resources.mailboxes.back().set_allow_overlay(
      video_frame->allow_overlay());
  external_resources.release_callbacks.push_back(
      base::Bind(&ReturnTexture, AsWeakPtr(), video_frame));
  return external_resources;
}

// static
void VideoResourceUpdater::RecycleResource(
    base::WeakPtr<VideoResourceUpdater> updater,
    ResourceProvider::ResourceId resource_id,
    uint32 sync_point,
    bool lost_resource,
    BlockingTaskRunner* main_thread_task_runner) {
  if (!updater.get()) {
    // Resource was already deleted.
    return;
  }

  const ResourceList::iterator resource_it = std::find_if(
      updater->all_resources_.begin(), updater->all_resources_.end(),
      [resource_id](const PlaneResource& plane_resource) {
        return plane_resource.resource_id == resource_id;
      });
  if (resource_it == updater->all_resources_.end())
    return;

  ContextProvider* context_provider = updater->context_provider_;
  if (context_provider && sync_point) {
    GLC(context_provider->ContextGL(),
        context_provider->ContextGL()->WaitSyncPointCHROMIUM(sync_point));
  }

  if (lost_resource) {
    resource_it->ref_count = 0;
    updater->DeleteResource(resource_it);
    return;
  }

  --resource_it->ref_count;
  DCHECK_GE(resource_it->ref_count, 0);
}

}  // namespace cc
