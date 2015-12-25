// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/buffer_format_util.h"

namespace content {

const int VideoCaptureBufferPool::kInvalidId = -1;

// A simple holder of a memory-backed buffer and accessors to it.
class SimpleBufferHandle final : public VideoCaptureBufferPool::BufferHandle {
 public:
  SimpleBufferHandle(void* data,
                     size_t mapped_size,
                     base::SharedMemoryHandle handle)
      : data_(data),
        mapped_size_(mapped_size)
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
        ,
        handle_(handle)
#endif
  {
  }
  ~SimpleBufferHandle() override {}

  gfx::Size dimensions() const override {
    NOTREACHED();
    return gfx::Size();
  }
  size_t mapped_size() const override { return mapped_size_; }
  void* data(int plane) override {
    DCHECK_EQ(plane, 0);
    return data_;
  }
  ClientBuffer AsClientBuffer(int plane) override {
    NOTREACHED();
    return nullptr;
  }
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  base::FileDescriptor AsPlatformFile() override {
    return handle_;
  }
#endif

 private:
  void* const data_;
  const size_t mapped_size_;
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  const base::SharedMemoryHandle handle_;
#endif
};

// A holder of a GpuMemoryBuffer-backed buffer. Holds weak references to
// GpuMemoryBuffer-backed buffers and provides accessors to their data.
class GpuMemoryBufferBufferHandle final
    : public VideoCaptureBufferPool::BufferHandle {
 public:
  GpuMemoryBufferBufferHandle(const gfx::Size& dimensions,
                              std::vector<
                                scoped_ptr<gfx::GpuMemoryBuffer>>* gmbs)
      : dimensions_(dimensions), gmbs_(gmbs) {
    DCHECK(gmbs);
  }
  ~GpuMemoryBufferBufferHandle() override {}

  gfx::Size dimensions() const override { return dimensions_; }
  size_t mapped_size() const override { return dimensions_.GetArea(); }
  void* data(int plane) override {
    DCHECK_GE(plane, 0);
    DCHECK_LT(plane, static_cast<int>(gmbs_->size()));
    DCHECK((*gmbs_)[plane]);
    return (*gmbs_)[plane]->memory(0);
  }
  ClientBuffer AsClientBuffer(int plane) override {
    DCHECK_GE(plane, 0);
    DCHECK_LT(plane, static_cast<int>(gmbs_->size()));
    return (*gmbs_)[plane]->AsClientBuffer();
  }
#if defined(OS_POSIX) && !(defined(OS_MACOSX) && !defined(OS_IOS))
  base::FileDescriptor AsPlatformFile() override {
    NOTREACHED();
    return base::FileDescriptor();
  }
#endif

 private:
  const gfx::Size dimensions_;
  std::vector<scoped_ptr<gfx::GpuMemoryBuffer>>* const gmbs_;
};

// Tracker specifics for SharedMemory.
class VideoCaptureBufferPool::SharedMemTracker final : public Tracker {
 public:
  SharedMemTracker();
  bool Init(media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            const gfx::Size& dimensions,
            base::Lock* lock) override;

  scoped_ptr<BufferHandle> GetBufferHandle() override {
    return make_scoped_ptr(new SimpleBufferHandle(
        shared_memory_.memory(), mapped_size_, shared_memory_.handle()));
  }
  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    return shared_memory_.ShareToProcess(process_handle, new_handle);
  }
  bool ShareToProcess2(int plane,
                       base::ProcessHandle process_handle,
                       gfx::GpuMemoryBufferHandle* new_handle) override {
    NOTREACHED();
    return false;
  }

 private:
  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
  size_t mapped_size_;
};

// Tracker specifics for GpuMemoryBuffer. Owns GpuMemoryBuffers and its
// associated pixel dimensions.
class VideoCaptureBufferPool::GpuMemoryBufferTracker final : public Tracker {
 public:
  GpuMemoryBufferTracker();
  bool Init(media::VideoPixelFormat format,
            media::VideoPixelStorage storage_type,
            const gfx::Size& dimensions,
            base::Lock* lock) override;
  ~GpuMemoryBufferTracker() override;

  scoped_ptr<BufferHandle> GetBufferHandle() override {
    DCHECK_EQ(gpu_memory_buffers_.size(),
              media::VideoFrame::NumPlanes(pixel_format()));
    return make_scoped_ptr(
        new GpuMemoryBufferBufferHandle(dimensions_, &gpu_memory_buffers_));
  }
  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    NOTREACHED();
    return false;
  }
  bool ShareToProcess2(int plane,
                       base::ProcessHandle process_handle,
                       gfx::GpuMemoryBufferHandle* new_handle) override;

 private:
  gfx::Size dimensions_;
  // Owned references to GpuMemoryBuffers.
  std::vector<scoped_ptr<gfx::GpuMemoryBuffer>> gpu_memory_buffers_;
};

VideoCaptureBufferPool::SharedMemTracker::SharedMemTracker() : Tracker() {}

bool VideoCaptureBufferPool::SharedMemTracker::Init(
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage_type,
    const gfx::Size& dimensions,
    base::Lock* lock) {
  DVLOG(2) << "allocating ShMem of " << dimensions.ToString();
  set_pixel_format(format);
  set_storage_type(storage_type);
  // |dimensions| can be 0x0 for trackers that do not require memory backing.
  set_pixel_count(dimensions.GetArea());
  mapped_size_ =
      media::VideoCaptureFormat(dimensions, 0.0f, format, storage_type)
          .ImageAllocationSize();
  if (!mapped_size_)
    return true;
  return shared_memory_.CreateAndMapAnonymous(mapped_size_);
}

VideoCaptureBufferPool::GpuMemoryBufferTracker::GpuMemoryBufferTracker()
    : Tracker() {
}

VideoCaptureBufferPool::GpuMemoryBufferTracker::~GpuMemoryBufferTracker() {
  for (const auto& gmb : gpu_memory_buffers_)
    gmb->Unmap();
}

bool VideoCaptureBufferPool::GpuMemoryBufferTracker::Init(
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage_type,
    const gfx::Size& dimensions,
    base::Lock* lock) {
  DVLOG(2) << "allocating GMB for " << dimensions.ToString();
  // BrowserGpuMemoryBufferManager::current() may not be accessed on IO Thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(BrowserGpuMemoryBufferManager::current());
  // This class is only expected to be called with I420 buffer requests at this
  // point.
  DCHECK_EQ(format, media::PIXEL_FORMAT_I420);
  set_pixel_format(format);
  set_storage_type(storage_type);
  set_pixel_count(dimensions.GetArea());
  // |dimensions| can be 0x0 for trackers that do not require memory backing.
  if (dimensions.GetArea() == 0u)
    return true;
  dimensions_ = dimensions;

  lock->Release();
  const size_t num_planes = media::VideoFrame::NumPlanes(pixel_format());
  for (size_t i = 0; i < num_planes; ++i) {
    const gfx::Size& size =
        media::VideoFrame::PlaneSize(pixel_format(), i, dimensions);
    gpu_memory_buffers_.push_back(
        BrowserGpuMemoryBufferManager::current()->AllocateGpuMemoryBuffer(
            size, gfx::BufferFormat::R_8,
            gfx::BufferUsage::GPU_READ_CPU_READ_WRITE));

    DLOG_IF(ERROR, !gpu_memory_buffers_[i]) << "Allocating GpuMemoryBuffer";
    if (!gpu_memory_buffers_[i] || !gpu_memory_buffers_[i]->Map())
      return false;
  }
  lock->Acquire();
  return true;
}

bool VideoCaptureBufferPool::GpuMemoryBufferTracker::ShareToProcess2(
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
    case gfx::SURFACE_TEXTURE_BUFFER:
    case gfx::OZONE_NATIVE_PIXMAP:
      *new_handle = current_gmb_handle;
      return true;
  }
  NOTREACHED();
  return true;
}

// static
scoped_ptr<VideoCaptureBufferPool::Tracker>
VideoCaptureBufferPool::Tracker::CreateTracker(
    media::VideoPixelStorage storage) {
  switch (storage) {
    case media::PIXEL_STORAGE_GPUMEMORYBUFFER:
      return make_scoped_ptr(new GpuMemoryBufferTracker());
    case media::PIXEL_STORAGE_CPU:
      return make_scoped_ptr(new SharedMemTracker());
  }
  NOTREACHED();
  return scoped_ptr<VideoCaptureBufferPool::Tracker>();
}

VideoCaptureBufferPool::Tracker::~Tracker() {}

VideoCaptureBufferPool::VideoCaptureBufferPool(int count)
    : count_(count),
      next_buffer_id_(0) {
  DCHECK_GT(count, 0);
}

VideoCaptureBufferPool::~VideoCaptureBufferPool() {
  STLDeleteValues(&trackers_);
}

bool VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle,
    base::SharedMemoryHandle* new_handle) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }
  if (tracker->ShareToProcess(process_handle, new_handle))
    return true;
  DPLOG(ERROR) << "Error mapping memory";
  return false;
}

bool VideoCaptureBufferPool::ShareToProcess2(
    int buffer_id,
    int plane,
    base::ProcessHandle process_handle,
    gfx::GpuMemoryBufferHandle* new_handle) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }
  if (tracker->ShareToProcess2(plane, process_handle, new_handle))
    return true;
  DPLOG(ERROR) << "Error mapping memory";
  return false;
}

scoped_ptr<VideoCaptureBufferPool::BufferHandle>
VideoCaptureBufferPool::GetBufferHandle(int buffer_id) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return scoped_ptr<BufferHandle>();
  }

  DCHECK(tracker->held_by_producer());
  return tracker->GetBufferHandle();
}

int VideoCaptureBufferPool::ReserveForProducer(
    media::VideoPixelFormat format,
    media::VideoPixelStorage storage,
    const gfx::Size& dimensions,
    int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(format, storage, dimensions,
                                    buffer_id_to_drop);
}

void VideoCaptureBufferPool::RelinquishProducerReservation(int buffer_id) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  tracker->set_held_by_producer(false);
}

void VideoCaptureBufferPool::HoldForConsumers(
    int buffer_id,
    int num_clients) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK(tracker->held_by_producer());
  DCHECK(!tracker->consumer_hold_count());

  tracker->set_consumer_hold_count(num_clients);
  // Note: |held_by_producer()| will stay true until
  // RelinquishProducerReservation() (usually called by destructor of the object
  // wrapping this tracker, e.g. a media::VideoFrame).
}

void VideoCaptureBufferPool::RelinquishConsumerHold(int buffer_id,
                                                    int num_clients) {
  base::AutoLock lock(lock_);
  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return;
  }
  DCHECK_GE(tracker->consumer_hold_count(), num_clients);

  tracker->set_consumer_hold_count(tracker->consumer_hold_count() -
                                   num_clients);
}

double VideoCaptureBufferPool::GetBufferPoolUtilization() const {
  base::AutoLock lock(lock_);
  int num_buffers_held = 0;
  for (const auto& entry : trackers_) {
    Tracker* const tracker = entry.second;
    if (tracker->held_by_producer() || tracker->consumer_hold_count() > 0)
      ++num_buffers_held;
  }
  return static_cast<double>(num_buffers_held) / count_;
}

int VideoCaptureBufferPool::ReserveForProducerInternal(
    media::VideoPixelFormat pixel_format,
    media::VideoPixelStorage storage_type,
    const gfx::Size& dimensions,
    int* buffer_id_to_drop) {
  lock_.AssertAcquired();

  const size_t size_in_pixels = dimensions.GetArea();
  // Look for a tracker that's allocated, big enough, and not in use. Track the
  // largest one that's not big enough, in case we have to reallocate a tracker.
  *buffer_id_to_drop = kInvalidId;
  size_t largest_size_in_pixels = 0;
  TrackerMap::iterator tracker_to_drop = trackers_.end();
  for (TrackerMap::iterator it = trackers_.begin(); it != trackers_.end();
       ++it) {
    Tracker* const tracker = it->second;
    if (!tracker->consumer_hold_count() && !tracker->held_by_producer()) {
      if (tracker->pixel_count() >= size_in_pixels &&
          (tracker->pixel_format() == pixel_format) &&
          (tracker->storage_type() == storage_type)) {
        // Existing tracker is big enough and has correct format. Reuse it.
        tracker->set_held_by_producer(true);
        return it->first;
      }
      if (tracker->pixel_count() > largest_size_in_pixels) {
        largest_size_in_pixels = tracker->pixel_count();
        tracker_to_drop = it;
      }
    }
  }

  // Preferably grow the pool by creating a new tracker. If we're at maximum
  // size, then reallocate by deleting an existing one instead.
  if (trackers_.size() == static_cast<size_t>(count_)) {
    if (tracker_to_drop == trackers_.end()) {
      // We're out of space, and can't find an unused tracker to reallocate.
      return kInvalidId;
    }
    *buffer_id_to_drop = tracker_to_drop->first;
    delete tracker_to_drop->second;
    trackers_.erase(tracker_to_drop);
  }

  // Create the new tracker.
  const int buffer_id = next_buffer_id_++;

  scoped_ptr<Tracker> tracker = Tracker::CreateTracker(storage_type);
  // TODO(emircan): We pass the lock here to solve GMB allocation issue, see
  // crbug.com/545238.
  if (!tracker->Init(pixel_format, storage_type, dimensions, &lock_)) {
    DLOG(ERROR) << "Error initializing Tracker";
    return kInvalidId;
  }

  tracker->set_held_by_producer(true);
  trackers_[buffer_id] = tracker.release();

  return buffer_id;
}

VideoCaptureBufferPool::Tracker* VideoCaptureBufferPool::GetTracker(
    int buffer_id) {
  TrackerMap::const_iterator it = trackers_.find(buffer_id);
  return (it == trackers_.end()) ? NULL : it->second;
}

}  // namespace content
