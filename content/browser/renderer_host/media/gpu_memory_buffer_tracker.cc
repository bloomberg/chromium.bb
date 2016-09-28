// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/gpu_memory_buffer_tracker.h"

#include "base/memory/ptr_util.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/browser/renderer_host/media/gpu_memory_buffer_handle.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_frame.h"

namespace content {

GpuMemoryBufferTracker::GpuMemoryBufferTracker()
    : VideoCaptureBufferTracker() {}

GpuMemoryBufferTracker::~GpuMemoryBufferTracker() {
  for (const auto& gmb : gpu_memory_buffers_)
    gmb->Unmap();
}

bool GpuMemoryBufferTracker::Init(const gfx::Size& dimensions,
                                  media::VideoPixelFormat format,
                                  media::VideoPixelStorage storage_type,
                                  base::Lock* lock) {
  DVLOG(2) << "allocating GMB for " << dimensions.ToString();
  // BrowserGpuMemoryBufferManager::current() may not be accessed on IO
  // Thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(BrowserGpuMemoryBufferManager::current());
  // This class is only expected to be called with I420 buffer requests at
  // this point.
  DCHECK_EQ(format, media::PIXEL_FORMAT_I420);
  set_dimensions(dimensions);
  set_max_pixel_count(dimensions.GetArea());
  set_pixel_format(format);
  set_storage_type(storage_type);
  // |dimensions| can be 0x0 for trackers that do not require memory backing.
  if (dimensions.GetArea() == 0u)
    return true;

  base::AutoUnlock auto_unlock(*lock);
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format());
  for (size_t i = 0; i < num_planes; ++i) {
    const gfx::Size& size =
        media::VideoFrame::PlaneSize(pixel_format(), i, dimensions);
    gpu_memory_buffers_.push_back(
        BrowserGpuMemoryBufferManager::current()->AllocateGpuMemoryBuffer(
            size, gfx::BufferFormat::R_8,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
            gpu::kNullSurfaceHandle));

    DLOG_IF(ERROR, !gpu_memory_buffers_[i]) << "Allocating GpuMemoryBuffer";
    if (!gpu_memory_buffers_[i] || !gpu_memory_buffers_[i]->Map())
      return false;
  }
  return true;
}

std::unique_ptr<media::VideoCaptureBufferHandle>
GpuMemoryBufferTracker::GetBufferHandle() {
  DCHECK_EQ(gpu_memory_buffers_.size(),
            media::VideoFrame::NumPlanes(pixel_format()));
  return base::MakeUnique<GpuMemoryBufferBufferHandle>(this);
}

bool GpuMemoryBufferTracker::ShareToProcess(
    base::ProcessHandle process_handle,
    base::SharedMemoryHandle* new_handle) {
  NOTREACHED();
  return false;
}

bool GpuMemoryBufferTracker::ShareToProcess2(
    int plane,
    base::ProcessHandle process_handle,
    gfx::GpuMemoryBufferHandle* new_handle) {
  DCHECK_LE(plane, static_cast<int>(gpu_memory_buffers_.size()));

  const auto& current_gmb_handle = gpu_memory_buffers_[plane]->GetHandle();
  switch (current_gmb_handle.type) {
    case gfx::EMPTY_BUFFER:
      NOTREACHED();
      return false;
    case gfx::SHARED_MEMORY_BUFFER: {
      DCHECK(base::SharedMemory::IsHandleValid(current_gmb_handle.handle));
      base::SharedMemory shared_memory(
          base::SharedMemory::DuplicateHandle(current_gmb_handle.handle),
          false);
      shared_memory.ShareToProcess(process_handle, &new_handle->handle);
      DCHECK(base::SharedMemory::IsHandleValid(new_handle->handle));
      new_handle->type = gfx::SHARED_MEMORY_BUFFER;
      return true;
    }
    case gfx::IO_SURFACE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      *new_handle = current_gmb_handle;
      return true;
  }
  NOTREACHED();
  return true;
}

}  // namespace content
