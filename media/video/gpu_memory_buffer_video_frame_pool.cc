// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/gpu_memory_buffer_video_frame_pool.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <list>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/containers/stack_container.h"
#include "base/location.h"
#include "base/memory/linked_ptr.h"
#include "base/trace_event/trace_event.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/renderers/gpu_video_accelerator_factories.h"

namespace media {

// Implementation of a pool of GpuMemoryBuffers used to back VideoFrames.
class GpuMemoryBufferVideoFramePool::PoolImpl
    : public base::RefCountedThreadSafe<
          GpuMemoryBufferVideoFramePool::PoolImpl> {
 public:
  // |media_task_runner| is the media task runner associated with the
  // GL context provided by |gpu_factories|
  // |worker_task_runner| is a task runner used to asynchronously copy
  // video frame's planes.
  // |gpu_factories| is an interface to GPU related operation and can be
  // null if a GL context is not available.
  PoolImpl(const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
           const scoped_refptr<base::TaskRunner>& worker_task_runner,
           const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories)
      : media_task_runner_(media_task_runner),
        worker_task_runner_(worker_task_runner),
        gpu_factories_(gpu_factories),
        texture_target_(gpu_factories ? gpu_factories->ImageTextureTarget()
                                      : GL_TEXTURE_2D) {
    DCHECK(media_task_runner_);
    DCHECK(worker_task_runner_);
  }

  // Takes a software VideoFrame and calls |frame_ready_cb| with a VideoFrame
  // backed by native textures if possible.
  // The data contained in video_frame is copied into the returned frame
  // asynchronously posting tasks to |worker_task_runner_|, while
  // |frame_ready_cb| will be called on |media_task_runner_| once all the data
  // has been copied.
  void CreateHardwareFrame(const scoped_refptr<VideoFrame>& video_frame,
                           const FrameReadyCB& cb);

 private:
  friend class base::RefCountedThreadSafe<
      GpuMemoryBufferVideoFramePool::PoolImpl>;
  ~PoolImpl();

  // Resource to represent a plane.
  struct PlaneResource {
    gfx::Size size;
    scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;
    unsigned texture_id = 0u;
    unsigned image_id = 0u;
    gpu::Mailbox mailbox;
  };

  // All the resources needed to compose a frame.
  struct FrameResources {
    FrameResources(VideoPixelFormat format, const gfx::Size& size)
        : format(format), size(size) {}
    bool in_use = true;
    VideoPixelFormat format;
    gfx::Size size;
    PlaneResource plane_resources[VideoFrame::kMaxPlanes];
  };

  // Copy |video_frame| data into |frame_resouces|
  // and calls |done| when done.
  void CopyVideoFrameToGpuMemoryBuffers(
      const scoped_refptr<VideoFrame>& video_frame,
      FrameResources* frame_resources,
      const FrameReadyCB& frame_ready_cb);

  // Called when all the data has been copied.
  void OnCopiesDone(const scoped_refptr<VideoFrame>& video_frame,
                    FrameResources* frame_resources,
                    const FrameReadyCB& frame_ready_cb);

  // Prepares GL resources, mailboxes and calls |frame_ready_cb| with the new
  // VideoFrame.
  // This has to be run on |media_task_runner_| where |frame_ready_cb| will also
  // be run.
  void BindAndCreateMailboxesHardwareFrameResources(
      const scoped_refptr<VideoFrame>& video_frame,
      FrameResources* frame_resources,
      const FrameReadyCB& frame_ready_cb);

  // Return true if |resources| can be used to represent a frame for
  // specific |format| and |size|.
  static bool AreFrameResourcesCompatible(const FrameResources* resources,
                                          const gfx::Size& size,
                                          VideoPixelFormat format) {
    return size == resources->size && format == resources->format;
  }

  // Get the resources needed for a frame out of the pool, or create them if
  // necessary.
  // This also drops the LRU resources that can't be reuse for this frame.
  FrameResources* GetOrCreateFrameResources(const gfx::Size& size,
                                            VideoPixelFormat format);

  // Callback called when a VideoFrame generated with GetFrameResources is no
  // longer referenced.
  // This could be called by any thread.
  void MailboxHoldersReleased(FrameResources* frame_resources,
                              uint32 sync_point);

  // Return frame resources to the pool. This has to be called on the thread
  // where |media_task_runner_| is current.
  void ReturnFrameResources(FrameResources* frame_resources);

  // Delete resources. This has to be called on the thread where |task_runner|
  // is current.
  static void DeleteFrameResources(
      const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
      FrameResources* frame_resources);

  // Task runner associated to the GL context provided by |gpu_factories_|.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  // Task runner used to asynchronously copy planes.
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  // Interface to GPU related operations.
  scoped_refptr<GpuVideoAcceleratorFactories> gpu_factories_;

  // Pool of resources.
  std::list<FrameResources*> resources_pool_;

  const unsigned texture_target_;
  DISALLOW_COPY_AND_ASSIGN(PoolImpl);
};

namespace {

// VideoFrame copies to GpuMemoryBuffers will be split in |kBytesPerCopyTarget|
// bytes copies and run in parallel.
const size_t kBytesPerCopyTarget = 1024 * 1024;  // 1MB

void CopyRowsToBuffer(int first_row,
                      int rows,
                      int bytes_per_row,
                      const uint8* source,
                      int source_stride,
                      uint8* output,
                      int dest_stride,
                      const base::Closure& done) {
  TRACE_EVENT2("media", "CopyRowsToBuffer", "bytes_per_row", bytes_per_row,
               "rows", rows);
  DCHECK_NE(dest_stride, 0);
  DCHECK_LE(bytes_per_row, std::abs(dest_stride));
  DCHECK_LE(bytes_per_row, source_stride);
  for (int row = first_row; row < first_row + rows; ++row) {
    memcpy(output + dest_stride * row, source + source_stride * row,
           bytes_per_row);
  }
  done.Run();
}

}  // unnamed namespace

// Creates a VideoFrame backed by native textures starting from a software
// VideoFrame.
// The data contained in |video_frame| is copied into the VideoFrame passed to
// |frame_ready_cb|.
// This has to be called on the thread where |media_task_runner_| is current.
void GpuMemoryBufferVideoFramePool::PoolImpl::CreateHardwareFrame(
    const scoped_refptr<VideoFrame>& video_frame,
    const FrameReadyCB& frame_ready_cb) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  if (!gpu_factories_ || !gpu_factories_->IsTextureRGSupported()) {
    frame_ready_cb.Run(video_frame);
    return;
  }
  switch (video_frame->format()) {
    // Supported cases.
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      break;
    // Unsupported cases.
    case PIXEL_FORMAT_YV12A:
    case PIXEL_FORMAT_YV16:
    case PIXEL_FORMAT_YV24:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_UNKNOWN:
      frame_ready_cb.Run(video_frame);
      return;
  }

  VideoPixelFormat format = video_frame->format();
  DCHECK(video_frame->visible_rect().origin().IsOrigin());
  const gfx::Size size = video_frame->visible_rect().size();

  // Acquire resources. Incompatible ones will be dropped from the pool.
  FrameResources* frame_resources = GetOrCreateFrameResources(size, format);
  if (!frame_resources) {
    frame_ready_cb.Run(video_frame);
    return;
  }

  worker_task_runner_->PostTask(
      FROM_HERE, base::Bind(&PoolImpl::CopyVideoFrameToGpuMemoryBuffers, this,
                            video_frame, frame_resources, frame_ready_cb));
}

void GpuMemoryBufferVideoFramePool::PoolImpl::OnCopiesDone(
    const scoped_refptr<VideoFrame>& video_frame,
    FrameResources* frame_resources,
    const FrameReadyCB& frame_ready_cb) {
  const VideoPixelFormat format = video_frame->format();
  const size_t planes = VideoFrame::NumPlanes(format);
  for (size_t i = 0; i < planes; ++i) {
    frame_resources->plane_resources[i].gpu_memory_buffer->Unmap();
  }

  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PoolImpl::BindAndCreateMailboxesHardwareFrameResources, this,
                 video_frame, frame_resources, frame_ready_cb));
}

// Copies |video_frame| into |frame_resources| asynchronously, posting n tasks
// that will be synchronized by a barrier.
// After the barrier is passed OnCopiesDone will be called.
void GpuMemoryBufferVideoFramePool::PoolImpl::CopyVideoFrameToGpuMemoryBuffers(
    const scoped_refptr<VideoFrame>& video_frame,
    FrameResources* frame_resources,
    const FrameReadyCB& frame_ready_cb) {
  const VideoPixelFormat format = video_frame->format();
  const size_t planes = VideoFrame::NumPlanes(format);
  gfx::Size size = video_frame->visible_rect().size();
  size_t copies = 0;
  for (size_t i = 0; i < planes; ++i) {
    int rows = VideoFrame::Rows(i, format, size.height());
    int bytes_per_row = VideoFrame::RowBytes(i, format, size.width());
    int rows_per_copy =
        std::max<size_t>(kBytesPerCopyTarget / bytes_per_row, 1);
    copies += rows / rows_per_copy;
    if (rows % rows_per_copy)
      ++copies;
  }

  base::Closure copies_done =
      base::Bind(&PoolImpl::OnCopiesDone, this, video_frame, frame_resources,
                 frame_ready_cb);
  base::Closure barrier = base::BarrierClosure(copies, copies_done);

  for (size_t i = 0; i < planes; ++i) {
    int rows = VideoFrame::Rows(i, format, size.height());
    int bytes_per_row = VideoFrame::RowBytes(i, format, size.width());
    int rows_per_copy =
        std::max<size_t>(kBytesPerCopyTarget / bytes_per_row, 1);

    PlaneResource& plane_resource = frame_resources->plane_resources[i];
    void* data = nullptr;
    CHECK(plane_resource.gpu_memory_buffer->Map(&data));
    uint8* mapped_buffer = static_cast<uint8*>(data);
    int dest_stride = 0;
    plane_resource.gpu_memory_buffer->GetStride(&dest_stride);

    for (int row = 0; row < rows; row += rows_per_copy) {
      worker_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&CopyRowsToBuffer, row,
                     std::min(rows_per_copy, rows - row), bytes_per_row,
                     video_frame->data(i), video_frame->stride(i),
                     mapped_buffer, dest_stride, barrier));
    }
  }
}

void GpuMemoryBufferVideoFramePool::PoolImpl::
    BindAndCreateMailboxesHardwareFrameResources(
        const scoped_refptr<VideoFrame>& video_frame,
        FrameResources* frame_resources,
        const FrameReadyCB& frame_ready_cb) {
  gpu::gles2::GLES2Interface* gles2 = gpu_factories_->GetGLES2Interface();
  if (!gles2) {
    frame_ready_cb.Run(video_frame);
    return;
  }

  const VideoPixelFormat format = video_frame->format();
  const size_t planes = VideoFrame::NumPlanes(format);
  const gfx::Size size = video_frame->visible_rect().size();
  gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
  // Set up the planes creating the mailboxes needed to refer to the textures.
  for (size_t i = 0; i < planes; ++i) {
    PlaneResource& plane_resource = frame_resources->plane_resources[i];
    // Bind the texture and create or rebind the image.
    gles2->BindTexture(texture_target_, plane_resource.texture_id);

    if (plane_resource.gpu_memory_buffer && !plane_resource.image_id) {
      const size_t width = VideoFrame::Columns(i, format, size.width());
      const size_t height = VideoFrame::Rows(i, format, size.height());
      plane_resource.image_id = gles2->CreateImageCHROMIUM(
          plane_resource.gpu_memory_buffer->AsClientBuffer(), width, height,
          GL_R8_EXT);
    } else {
      gles2->ReleaseTexImage2DCHROMIUM(texture_target_,
                                       plane_resource.image_id);
    }
    gles2->BindTexImage2DCHROMIUM(texture_target_, plane_resource.image_id);
    mailbox_holders[i] =
        gpu::MailboxHolder(plane_resource.mailbox, texture_target_, 0);
  }

  // Insert a sync_point, this is needed to make sure that the textures the
  // mailboxes refer to will be used only after all the previous commands posted
  // in the command buffer have been processed.
  unsigned sync_point = gles2->InsertSyncPointCHROMIUM();
  for (size_t i = 0; i < planes; ++i) {
    mailbox_holders[i].sync_point = sync_point;
  }

  // Create the VideoFrame backed by native textures.
  scoped_refptr<VideoFrame> frame = VideoFrame::WrapYUV420NativeTextures(
      mailbox_holders[VideoFrame::kYPlane],
      mailbox_holders[VideoFrame::kUPlane],
      mailbox_holders[VideoFrame::kVPlane],
      base::Bind(&PoolImpl::MailboxHoldersReleased, this, frame_resources),
      size, video_frame->visible_rect(), video_frame->natural_size(),
      video_frame->timestamp());
  if (video_frame->metadata()->IsTrue(VideoFrameMetadata::ALLOW_OVERLAY))
    frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  frame_ready_cb.Run(frame);
}

// Destroy all the resources posting one task per FrameResources
// to the |media_task_runner_|.
GpuMemoryBufferVideoFramePool::PoolImpl::~PoolImpl() {
  // Delete all the resources on the media thread.
  while (!resources_pool_.empty()) {
    FrameResources* frame_resources = resources_pool_.front();
    resources_pool_.pop_front();
    media_task_runner_->PostTask(
        FROM_HERE, base::Bind(&PoolImpl::DeleteFrameResources, gpu_factories_,
                              base::Owned(frame_resources)));
  }
}

// Tries to find the resources in the pool or create them.
// Incompatible resources will be dropped.
GpuMemoryBufferVideoFramePool::PoolImpl::FrameResources*
GpuMemoryBufferVideoFramePool::PoolImpl::GetOrCreateFrameResources(
    const gfx::Size& size,
    VideoPixelFormat format) {
  auto it = resources_pool_.begin();
  while (it != resources_pool_.end()) {
    FrameResources* frame_resources = *it;
    if (!frame_resources->in_use) {
      if (AreFrameResourcesCompatible(frame_resources, size, format)) {
        frame_resources->in_use = true;
        return frame_resources;
      } else {
        resources_pool_.erase(it++);
        DeleteFrameResources(gpu_factories_, frame_resources);
        delete frame_resources;
      }
    } else {
      it++;
    }
  }

  // Create the resources.
  gpu::gles2::GLES2Interface* gles2 = gpu_factories_->GetGLES2Interface();
  if (!gles2)
    return nullptr;
  gles2->ActiveTexture(GL_TEXTURE0);
  size_t planes = VideoFrame::NumPlanes(format);
  FrameResources* frame_resources = new FrameResources(format, size);
  resources_pool_.push_back(frame_resources);
  for (size_t i = 0; i < planes; ++i) {
    PlaneResource& plane_resource = frame_resources->plane_resources[i];
    const size_t width = VideoFrame::Columns(i, format, size.width());
    const size_t height = VideoFrame::Rows(i, format, size.height());
    const gfx::Size plane_size(width, height);
    plane_resource.gpu_memory_buffer = gpu_factories_->AllocateGpuMemoryBuffer(
        plane_size, gfx::BufferFormat::R_8, gfx::BufferUsage::MAP);

    gles2->GenTextures(1, &plane_resource.texture_id);
    gles2->BindTexture(texture_target_, plane_resource.texture_id);
    gles2->TexParameteri(texture_target_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gles2->TexParameteri(texture_target_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gles2->GenMailboxCHROMIUM(plane_resource.mailbox.name);
    gles2->ProduceTextureCHROMIUM(texture_target_, plane_resource.mailbox.name);
  }
  return frame_resources;
}

// static
void GpuMemoryBufferVideoFramePool::PoolImpl::DeleteFrameResources(
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    FrameResources* frame_resources) {
  // TODO(dcastagna): As soon as the context lost is dealt with in media,
  // make sure that we won't execute this callback (use a weak pointer to
  // the old context).
  gpu::gles2::GLES2Interface* gles2 = gpu_factories->GetGLES2Interface();
  if (!gles2)
    return;

  for (PlaneResource& plane_resource : frame_resources->plane_resources) {
    if (plane_resource.image_id)
      gles2->DestroyImageCHROMIUM(plane_resource.image_id);
    if (plane_resource.texture_id)
      gles2->DeleteTextures(1, &plane_resource.texture_id);
  }
}

// Called when a VideoFrame is no longer references.
void GpuMemoryBufferVideoFramePool::PoolImpl::MailboxHoldersReleased(
    FrameResources* frame_resources,
    uint32 sync_point) {
  // Return the resource on the media thread.
  media_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&PoolImpl::ReturnFrameResources, this, frame_resources));
}

// Put back the resoruces in the pool.
void GpuMemoryBufferVideoFramePool::PoolImpl::ReturnFrameResources(
    FrameResources* frame_resources) {

  auto it = std::find(resources_pool_.begin(), resources_pool_.end(),
                      frame_resources);
  DCHECK(it != resources_pool_.end());
  // We want the pool to behave in a FIFO way.
  // This minimizes the chances of locking the buffer that might be
  // still needed for drawing.
  std::swap(*it, resources_pool_.back());
  frame_resources->in_use = false;
}

GpuMemoryBufferVideoFramePool::GpuMemoryBufferVideoFramePool(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories)
    : pool_impl_(
          new PoolImpl(media_task_runner, worker_task_runner, gpu_factories)) {}

GpuMemoryBufferVideoFramePool::~GpuMemoryBufferVideoFramePool() {
}

void GpuMemoryBufferVideoFramePool::MaybeCreateHardwareFrame(
    const scoped_refptr<VideoFrame>& video_frame,
    const FrameReadyCB& frame_ready_cb) {
  DCHECK(video_frame);
  pool_impl_->CreateHardwareFrame(video_frame, frame_ready_cb);
}

}  // namespace media
