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
#include "cc/resources/resource_provider.h"
#include "cc/resources/resource_util.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "media/video/half_float_maker.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {

namespace {

const viz::ResourceFormat kRGBResourceFormat = viz::RGBA_8888;

VideoFrameExternalResources::ResourceType ResourceTypeForVideoFrame(
    media::VideoFrame* video_frame,
    gfx::BufferFormat* buffer_format,
    bool use_stream_video_draw_quad) {
  DCHECK(buffer_format);
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_ARGB:
    case media::PIXEL_FORMAT_XRGB:
    case media::PIXEL_FORMAT_UYVY:
      switch (video_frame->mailbox_holder(0).texture_target) {
        case GL_TEXTURE_EXTERNAL_OES:
          if (use_stream_video_draw_quad &&
              !video_frame->metadata()->IsTrue(
                  media::VideoFrameMetadata::COPY_REQUIRED))
            return VideoFrameExternalResources::STREAM_TEXTURE_RESOURCE;
        case GL_TEXTURE_2D:
          return (video_frame->format() == media::PIXEL_FORMAT_XRGB)
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
      break;
    case media::PIXEL_FORMAT_NV12:
      switch (video_frame->mailbox_holder(0).texture_target) {
        case GL_TEXTURE_EXTERNAL_OES:
        case GL_TEXTURE_2D:
          // Single plane textures can be sampled as RGB.
          if (video_frame->NumTextures() > 1) {
            return VideoFrameExternalResources::YUV_RESOURCE;
          } else {
            *buffer_format = gfx::BufferFormat::YUV_420_BIPLANAR;
            return VideoFrameExternalResources::RGB_RESOURCE;
          }
        case GL_TEXTURE_RECTANGLE_ARB:
          return VideoFrameExternalResources::RGB_RESOURCE;
        default:
          NOTREACHED();
          break;
      }
      break;
    case media::PIXEL_FORMAT_YV12:
    case media::PIXEL_FORMAT_YV16:
    case media::PIXEL_FORMAT_YV24:
    case media::PIXEL_FORMAT_YV12A:
    case media::PIXEL_FORMAT_NV21:
    case media::PIXEL_FORMAT_YUY2:
    case media::PIXEL_FORMAT_RGB24:
    case media::PIXEL_FORMAT_RGB32:
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
    case media::PIXEL_FORMAT_Y8:
    case media::PIXEL_FORMAT_Y16:
    case media::PIXEL_FORMAT_I422:
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
      const uint64_t fence_sync = gl_->InsertFenceSyncCHROMIUM();
      gl_->ShallowFlushCHROMIUM();
      gl_->GenSyncTokenCHROMIUM(fence_sync, sync_token->GetData());
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
  const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
  gl->OrderingBarrierCHROMIUM();
  gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token->GetData());
}

}  // namespace

VideoResourceUpdater::PlaneResource::PlaneResource(
    unsigned int resource_id,
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    gpu::Mailbox mailbox)
    : resource_id_(resource_id),
      resource_size_(resource_size),
      resource_format_(resource_format),
      mailbox_(mailbox) {}

VideoResourceUpdater::PlaneResource::PlaneResource(const PlaneResource& other) =
    default;

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

VideoFrameExternalResources::VideoFrameExternalResources()
    : type(NONE),
      read_lock_fences_enabled(false),
      buffer_format(gfx::BufferFormat::RGBA_8888),
      offset(0.0f),
      multiplier(1.0f),
      bits_per_channel(8) {}

VideoFrameExternalResources::VideoFrameExternalResources(
    const VideoFrameExternalResources& other) = default;

VideoFrameExternalResources::~VideoFrameExternalResources() {}

VideoResourceUpdater::VideoResourceUpdater(
    viz::ContextProvider* context_provider,
    ResourceProvider* resource_provider,
    bool use_stream_video_draw_quad)
    : context_provider_(context_provider),
      resource_provider_(resource_provider),
      use_stream_video_draw_quad_(use_stream_video_draw_quad),
      weak_ptr_factory_(this) {}

VideoResourceUpdater::~VideoResourceUpdater() {
  for (const PlaneResource& plane_resource : all_resources_)
    resource_provider_->DeleteResource(plane_resource.resource_id());
}

VideoResourceUpdater::ResourceList::iterator
VideoResourceUpdater::RecycleOrAllocateResource(
    const gfx::Size& resource_size,
    viz::ResourceFormat resource_format,
    const gfx::ColorSpace& color_space,
    bool software_resource,
    bool immutable_hint,
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
        it->mailbox().IsZero() == software_resource &&
        resource_provider_->IsImmutable(it->resource_id()) == immutable_hint) {
      recyclable_resource = it;
    }
  }

  if (recyclable_resource != all_resources_.end())
    return recyclable_resource;

  // There was nothing available to reuse or recycle. Allocate a new resource.
  return AllocateResource(resource_size, resource_format, color_space,
                          !software_resource, immutable_hint);
}

VideoResourceUpdater::ResourceList::iterator
VideoResourceUpdater::AllocateResource(const gfx::Size& plane_size,
                                       viz::ResourceFormat format,
                                       const gfx::ColorSpace& color_space,
                                       bool has_mailbox,
                                       bool immutable_hint) {
  // TODO(danakj): Abstract out hw/sw resource create/delete from
  // ResourceProvider and stop using ResourceProvider in this class.
  const viz::ResourceId resource_id = resource_provider_->CreateResource(
      plane_size,
      immutable_hint ? ResourceProvider::TEXTURE_HINT_IMMUTABLE
                     : ResourceProvider::TEXTURE_HINT_DEFAULT,
      format, color_space);
  DCHECK_NE(resource_id, 0u);

  gpu::Mailbox mailbox;
  if (has_mailbox) {
    DCHECK(context_provider_);

    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

    gl->GenMailboxCHROMIUM(mailbox.name);
    ResourceProvider::ScopedWriteLockGL lock(resource_provider_, resource_id);
    gl->ProduceTextureDirectCHROMIUM(
        lock.GetTexture(),
        resource_provider_->GetResourceTextureTarget(resource_id),
        mailbox.name);
  }
  all_resources_.push_front(
      PlaneResource(resource_id, plane_size, format, mailbox));
  return all_resources_.begin();
}

void VideoResourceUpdater::DeleteResource(ResourceList::iterator resource_it) {
  DCHECK(!resource_it->has_refs());
  resource_provider_->DeleteResource(resource_it->resource_id());
  all_resources_.erase(resource_it);
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

// For frames that we receive in software format, determine the dimensions of
// each plane in the frame.
static gfx::Size SoftwarePlaneDimension(media::VideoFrame* input_frame,
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

VideoFrameExternalResources VideoResourceUpdater::CreateForSoftwarePlanes(
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForSoftwarePlanes");
  const media::VideoPixelFormat input_frame_format = video_frame->format();

  int bits_per_channel = video_frame->BitsPerChannel(input_frame_format);

  // Only YUV and Y16 software video frames are supported.
  DCHECK(media::IsYuvPlanar(input_frame_format) ||
         input_frame_format == media::PIXEL_FORMAT_Y16);

  const bool software_compositor = context_provider_ == NULL;

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
    output_resource_format = kRGBResourceFormat;
    output_plane_count = 1;
    bits_per_channel = 8;
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

    const bool is_immutable = true;
    ResourceList::iterator resource_it = RecycleOrAllocateResource(
        output_plane_resource_size, output_resource_format, output_color_space,
        software_compositor, is_immutable, video_frame->unique_id(), i);

    resource_it->add_ref();
    plane_resources.push_back(resource_it);
  }

  VideoFrameExternalResources external_resources;

  external_resources.bits_per_channel = bits_per_channel;

  if (software_compositor || texture_needs_rgb_conversion) {
    DCHECK_EQ(plane_resources.size(), 1u);
    PlaneResource& plane_resource = *plane_resources[0];
    DCHECK_EQ(plane_resource.resource_format(), kRGBResourceFormat);
    DCHECK_EQ(software_compositor, plane_resource.mailbox().IsZero());

    if (!plane_resource.Matches(video_frame->unique_id(), 0)) {
      // We need to transfer data from |video_frame| to the plane resource.
      if (software_compositor) {
        if (!video_renderer_)
          video_renderer_.reset(new media::SkCanvasVideoRenderer);

        ResourceProvider::ScopedWriteLockSoftware lock(
            resource_provider_, plane_resource.resource_id());
        SkiaPaintCanvas canvas(lock.sk_bitmap());
        // This is software path, so canvas and video_frame are always backed
        // by software.
        video_renderer_->Copy(video_frame, &canvas, media::Context3D());
      } else {
        size_t bytes_per_row = ResourceUtil::CheckedWidthInBytes<size_t>(
            video_frame->coded_size().width(), viz::ResourceFormat::RGBA_8888);
        size_t needed_size = bytes_per_row * video_frame->coded_size().height();
        if (upload_pixels_.size() < needed_size)
          upload_pixels_.resize(needed_size);

        media::SkCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
            video_frame.get(), &upload_pixels_[0], bytes_per_row);

        resource_provider_->CopyToResource(plane_resource.resource_id(),
                                           &upload_pixels_[0],
                                           plane_resource.resource_size());
      }
      plane_resource.SetUniqueId(video_frame->unique_id(), 0);
    }

    if (software_compositor) {
      external_resources.software_resources.push_back(
          plane_resource.resource_id());
      external_resources.software_release_callback =
          base::Bind(&RecycleResource, weak_ptr_factory_.GetWeakPtr(),
                     plane_resource.resource_id());
      external_resources.type = VideoFrameExternalResources::SOFTWARE_RESOURCE;
    } else {
      // Set the sync token otherwise resource is assumed to be synchronized.
      gpu::SyncToken sync_token;
      GenerateCompositorSyncToken(context_provider_->ContextGL(), &sync_token);

      viz::TextureMailbox mailbox(plane_resource.mailbox(), sync_token,
                                  resource_provider_->GetResourceTextureTarget(
                                      plane_resource.resource_id()));
      mailbox.set_color_space(output_color_space);

      external_resources.mailboxes.push_back(mailbox);
      external_resources.release_callbacks.push_back(
          base::Bind(&RecycleResource, weak_ptr_factory_.GetWeakPtr(),
                     plane_resource.resource_id()));
      external_resources.type = VideoFrameExternalResources::RGBA_RESOURCE;
    }
    return external_resources;
  }

  std::unique_ptr<media::HalfFloatMaker> half_float_maker;
  if (resource_provider_->YuvResourceFormat(bits_per_channel) ==
      viz::LUMINANCE_F16) {
    half_float_maker =
        media::HalfFloatMaker::NewHalfFloatMaker(bits_per_channel);
    external_resources.offset = half_float_maker->Offset();
    external_resources.multiplier = half_float_maker->Multiplier();
  }

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    PlaneResource& plane_resource = *plane_resources[i];
    // Update each plane's resource id with its content.
    DCHECK_EQ(plane_resource.resource_format(),
              resource_provider_->YuvResourceFormat(bits_per_channel));

    if (!plane_resource.Matches(video_frame->unique_id(), i)) {
      // TODO(hubbe): Move upload code to media/.
      // We need to transfer data from |video_frame| to the plane resource.
      // TODO(reveman): Can use GpuMemoryBuffers here to improve performance.

      // The |resource_size_pixels| is the size of the resource we want to
      // upload to.
      gfx::Size resource_size_pixels = plane_resource.resource_size();
      // The |video_stride_bytes| is the width of the video frame we are
      // uploading (including non-frame data to fill in the stride).
      int video_stride_bytes = video_frame->stride(i);

      size_t bytes_per_row = ResourceUtil::CheckedWidthInBytes<size_t>(
          resource_size_pixels.width(), plane_resource.resource_format());
      // Use 4-byte row alignment (OpenGL default) for upload performance.
      // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
      size_t upload_image_stride =
          MathUtil::CheckedRoundUp<size_t>(bytes_per_row, 4u);

      bool needs_conversion = false;
      int shift = 0;

      // viz::LUMINANCE_F16 uses half-floats, so we always need a conversion
      // step.
      if (plane_resource.resource_format() == viz::LUMINANCE_F16) {
        needs_conversion = true;
      } else if (bits_per_channel > 8) {
        // If bits_per_channel > 8 and we can't use viz::LUMINANCE_F16, we need
        // to shift the data down and create an 8-bit texture.
        needs_conversion = true;
        shift = bits_per_channel - 8;
      }
      const uint8_t* pixels;
      if (static_cast<int>(upload_image_stride) == video_stride_bytes &&
          !needs_conversion) {
        pixels = video_frame->data(i);
      } else {
        // Avoid malloc for each frame/plane if possible.
        size_t needed_size =
            upload_image_stride * resource_size_pixels.height();
        if (upload_pixels_.size() < needed_size)
          upload_pixels_.resize(needed_size);

        for (int row = 0; row < resource_size_pixels.height(); ++row) {
          if (plane_resource.resource_format() == viz::LUMINANCE_F16) {
            uint16_t* dst = reinterpret_cast<uint16_t*>(
                &upload_pixels_[upload_image_stride * row]);
            const uint16_t* src = reinterpret_cast<uint16_t*>(
                video_frame->data(i) + (video_stride_bytes * row));
            half_float_maker->MakeHalfFloats(src, bytes_per_row / 2, dst);
          } else if (shift != 0) {
            // We have more-than-8-bit input which we need to shift
            // down to fit it into an 8-bit texture.
            uint8_t* dst = &upload_pixels_[upload_image_stride * row];
            const uint16_t* src = reinterpret_cast<uint16_t*>(
                video_frame->data(i) + (video_stride_bytes * row));
            for (size_t i = 0; i < bytes_per_row; i++)
              dst[i] = src[i] >> shift;
          } else {
            // Input and output are the same size and format, but
            // differ in stride, copy one row at a time.
            uint8_t* dst = &upload_pixels_[upload_image_stride * row];
            const uint8_t* src =
                video_frame->data(i) + (video_stride_bytes * row);
            memcpy(dst, src, bytes_per_row);
          }
        }
        pixels = &upload_pixels_[0];
      }

      resource_provider_->CopyToResource(plane_resource.resource_id(), pixels,
                                         resource_size_pixels);
      plane_resource.SetUniqueId(video_frame->unique_id(), i);
    }
  }

  // Set the sync token otherwise resource is assumed to be synchronized.
  gpu::SyncToken sync_token;
  GenerateCompositorSyncToken(context_provider_->ContextGL(), &sync_token);

  for (size_t i = 0; i < plane_resources.size(); ++i) {
    PlaneResource& plane_resource = *plane_resources[i];
    // VideoResourceUpdater shares a context with the compositor so a
    // sync token is not required.
    viz::TextureMailbox mailbox(plane_resource.mailbox(), sync_token,
                                resource_provider_->GetResourceTextureTarget(
                                    plane_resource.resource_id()));
    mailbox.set_color_space(output_color_space);
    external_resources.mailboxes.push_back(mailbox);
    external_resources.release_callbacks.push_back(
        base::Bind(&RecycleResource, weak_ptr_factory_.GetWeakPtr(),
                   plane_resource.resource_id()));
  }

  external_resources.type = VideoFrameExternalResources::YUV_RESOURCE;
  return external_resources;
}

// static
void VideoResourceUpdater::ReturnTexture(
    base::WeakPtr<VideoResourceUpdater> updater,
    const scoped_refptr<media::VideoFrame>& video_frame,
    const gpu::SyncToken& sync_token,
    bool lost_resource,
    BlockingTaskRunner* main_thread_task_runner) {
  // TODO(dshwang) this case should be forwarded to the decoder as lost
  // resource.
  if (lost_resource || !updater.get())
    return;
  // The video frame will insert a wait on the previous release sync token.
  SyncTokenClientImpl client(updater->context_provider_->ContextGL(),
                             sync_token);
  video_frame->UpdateReleaseSyncToken(&client);
}

// Create a copy of a texture-backed source video frame in a new GL_TEXTURE_2D
// texture.
void VideoResourceUpdater::CopyPlaneTexture(
    media::VideoFrame* video_frame,
    const gfx::ColorSpace& resource_color_space,
    const gpu::MailboxHolder& mailbox_holder,
    VideoFrameExternalResources* external_resources) {
  const gfx::Size output_plane_resource_size = video_frame->coded_size();
  // The copy needs to be a direct transfer of pixel data, so we use an RGBA8
  // target to avoid loss of precision or dropping any alpha component.
  const viz::ResourceFormat copy_target_format = viz::ResourceFormat::RGBA_8888;

  const bool is_immutable = false;
  const int no_unique_id = 0;
  const int no_plane_index = -1;  // Do not recycle referenced textures.
  VideoResourceUpdater::ResourceList::iterator resource =
      RecycleOrAllocateResource(output_plane_resource_size, copy_target_format,
                                resource_color_space, false, is_immutable,
                                no_unique_id, no_plane_index);
  resource->add_ref();

  ResourceProvider::ScopedWriteLockGL lock(resource_provider_,
                                           resource->resource_id());
  DCHECK_EQ(
      resource_provider_->GetResourceTextureTarget(resource->resource_id()),
      (GLenum)GL_TEXTURE_2D);

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();

  gl->WaitSyncTokenCHROMIUM(mailbox_holder.sync_token.GetConstData());
  uint32_t src_texture_id = gl->CreateAndConsumeTextureCHROMIUM(
      mailbox_holder.texture_target, mailbox_holder.mailbox.name);
  gl->CopySubTextureCHROMIUM(
      src_texture_id, 0, GL_TEXTURE_2D, lock.GetTexture(), 0, 0, 0, 0, 0,
      output_plane_resource_size.width(), output_plane_resource_size.height(),
      false, false, false);
  gl->DeleteTextures(1, &src_texture_id);

  // Pass an empty sync token to force generation of a new sync token.
  SyncTokenClientImpl client(gl, gpu::SyncToken());
  gpu::SyncToken sync_token = video_frame->UpdateReleaseSyncToken(&client);

  // Set sync token otherwise resource is assumed to be synchronized.
  viz::TextureMailbox mailbox(resource->mailbox(), sync_token, GL_TEXTURE_2D,
                              video_frame->coded_size(), false, false);
  mailbox.set_color_space(resource_color_space);
  external_resources->mailboxes.push_back(mailbox);

  external_resources->release_callbacks.push_back(
      base::Bind(&RecycleResource, weak_ptr_factory_.GetWeakPtr(),
                 resource->resource_id()));
}

VideoFrameExternalResources VideoResourceUpdater::CreateForHardwarePlanes(
    scoped_refptr<media::VideoFrame> video_frame) {
  TRACE_EVENT0("cc", "VideoResourceUpdater::CreateForHardwarePlanes");
  DCHECK(video_frame->HasTextures());
  if (!context_provider_)
    return VideoFrameExternalResources();

  VideoFrameExternalResources external_resources;
  if (video_frame->metadata()->IsTrue(
          media::VideoFrameMetadata::READ_LOCK_FENCES_ENABLED)) {
    external_resources.read_lock_fences_enabled = true;
  }
  gfx::ColorSpace resource_color_space = video_frame->ColorSpace();

  external_resources.type = ResourceTypeForVideoFrame(
      video_frame.get(), &external_resources.buffer_format,
      use_stream_video_draw_quad_);
  if (external_resources.type == VideoFrameExternalResources::NONE) {
    DLOG(ERROR) << "Unsupported Texture format"
                << media::VideoPixelFormatToString(video_frame->format());
    return external_resources;
  }
  if (external_resources.type == VideoFrameExternalResources::RGB_RESOURCE)
    resource_color_space = resource_color_space.GetAsFullRangeRGB();

  const size_t num_planes = media::VideoFrame::NumPlanes(video_frame->format());
  for (size_t i = 0; i < num_planes; ++i) {
    const gpu::MailboxHolder& mailbox_holder = video_frame->mailbox_holder(i);
    if (mailbox_holder.mailbox.IsZero())
      break;

    if (video_frame->metadata()->IsTrue(
            media::VideoFrameMetadata::COPY_REQUIRED)) {
      CopyPlaneTexture(video_frame.get(), resource_color_space, mailbox_holder,
                       &external_resources);
    } else {
      viz::TextureMailbox mailbox(
          mailbox_holder.mailbox, mailbox_holder.sync_token,
          mailbox_holder.texture_target, video_frame->coded_size(),
          video_frame->metadata()->IsTrue(
              media::VideoFrameMetadata::ALLOW_OVERLAY),
          false);
      mailbox.set_color_space(resource_color_space);
#if defined(OS_ANDROID)
      mailbox.set_is_backed_by_surface_texture(video_frame->metadata()->IsTrue(
          media::VideoFrameMetadata::SURFACE_TEXTURE));
      mailbox.set_wants_promotion_hint(video_frame->metadata()->IsTrue(
          media::VideoFrameMetadata::WANTS_PROMOTION_HINT));
#endif
      external_resources.mailboxes.push_back(mailbox);
      external_resources.release_callbacks.push_back(base::Bind(
          &ReturnTexture, weak_ptr_factory_.GetWeakPtr(), video_frame));
    }
  }
  return external_resources;
}

// static
void VideoResourceUpdater::RecycleResource(
    base::WeakPtr<VideoResourceUpdater> updater,
    viz::ResourceId resource_id,
    const gpu::SyncToken& sync_token,
    bool lost_resource,
    BlockingTaskRunner* main_thread_task_runner) {
  if (!updater.get()) {
    // Resource was already deleted.
    return;
  }
  const ResourceList::iterator resource_it = std::find_if(
      updater->all_resources_.begin(), updater->all_resources_.end(),
      [resource_id](const PlaneResource& plane_resource) {
        return plane_resource.resource_id() == resource_id;
      });
  if (resource_it == updater->all_resources_.end())
    return;

  viz::ContextProvider* context_provider = updater->context_provider_;
  if (context_provider && sync_token.HasData()) {
    context_provider->ContextGL()->WaitSyncTokenCHROMIUM(
        sync_token.GetConstData());
  }

  if (lost_resource) {
    resource_it->clear_refs();
    updater->DeleteResource(resource_it);
    return;
  }

  resource_it->remove_ref();
}

}  // namespace cc
