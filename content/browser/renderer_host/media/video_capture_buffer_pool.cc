// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "content/browser/gpu/browser_gpu_memory_buffer_manager.h"
#include "content/common/gpu/client/gpu_memory_buffer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "media/base/video_frame.h"

using media::VideoFrame;

namespace content {

const int VideoCaptureBufferPool::kInvalidId = -1;

VideoFrame::Format VideoPixelFormatToVideoFrameFormat(
    media::VideoPixelFormat pixel_format) {
  static struct {
    media::VideoPixelFormat pixel_format;
    VideoFrame::Format frame_format;
  } const kVideoPixelFormatToVideoFrameFormat[] = {
      {media::PIXEL_FORMAT_I420, VideoFrame::I420},
      {media::PIXEL_FORMAT_TEXTURE, VideoFrame::ARGB},
      {media::PIXEL_FORMAT_GPUMEMORYBUFFER, VideoFrame::ARGB},
  };

  for (const auto& format_pair : kVideoPixelFormatToVideoFrameFormat) {
    if (format_pair.pixel_format == pixel_format)
      return format_pair.frame_format;
  }
  LOG(ERROR) << "Unsupported VideoPixelFormat "
             << media::VideoCaptureFormat::PixelFormatToString(pixel_format);
  return VideoFrame::UNKNOWN;
}

VideoFrame::StorageType VideoPixelFormatToVideoFrameStorageType(
    media::VideoPixelFormat pixel_format) {
  static struct {
    media::VideoPixelFormat pixel_format;
    VideoFrame::StorageType storage_type;
  } const kVideoPixelFormatToVideoFrameStorageType[] = {
      {media::PIXEL_FORMAT_I420, VideoFrame::STORAGE_SHMEM},
      {media::PIXEL_FORMAT_TEXTURE, VideoFrame::STORAGE_OPAQUE},
      {media::PIXEL_FORMAT_GPUMEMORYBUFFER, VideoFrame::STORAGE_OPAQUE},
  };

  for (const auto& format_pair : kVideoPixelFormatToVideoFrameStorageType) {
    if (format_pair.pixel_format == pixel_format)
      return format_pair.storage_type;
  }
  LOG(ERROR) << "Unsupported VideoPixelFormat "
             << media::VideoCaptureFormat::PixelFormatToString(pixel_format);
  return VideoFrame::STORAGE_UNKNOWN;
}

// A simple holder of a memory-backed buffer and accesors to it.
class SimpleBufferHandle final : public VideoCaptureBufferPool::BufferHandle {
 public:
  SimpleBufferHandle(void* data, size_t size, base::SharedMemoryHandle handle)
      : data_(data), size_(size), handle_(handle) {}
  ~SimpleBufferHandle() override {}

  size_t size() const override { return size_; }
  void* data() override { return data_; }
  gfx::GpuMemoryBufferType GetType() override {
    return gfx::SHARED_MEMORY_BUFFER;
  }
  ClientBuffer AsClientBuffer() override { return nullptr; }
  base::PlatformFile AsPlatformFile() override {
#if defined(OS_POSIX)
    return handle_.fd;
#elif defined(OS_WIN)
    return handle_;
#endif
  }

 private:
  void* const data_;
  const size_t size_;
  const base::SharedMemoryHandle handle_;
};

// A holder of a GpuMemoryBuffer-backed buffer, Map()ed on ctor and Unmap()ed on
// dtor. Holds a weak reference to its GpuMemoryBuffer.
// TODO(mcasas) Map()ed on ctor, or on first use?
class GpuMemoryBufferBufferHandle
    final : public VideoCaptureBufferPool::BufferHandle {
 public:
  GpuMemoryBufferBufferHandle(gfx::GpuMemoryBuffer* gmb, size_t size)
      : gmb_(gmb),
        data_(new void* [GpuMemoryBufferImpl::
                             NumberOfPlanesForGpuMemoryBufferFormat(
                                 gmb_->GetFormat())]),
        size_(size) {
    DCHECK(gmb && !gmb_->IsMapped());
    gmb_->Map(data_.get());
  }
  ~GpuMemoryBufferBufferHandle() override { gmb_->Unmap(); }

  size_t size() const override { return size_; }
  void* data() override { return data_[0]; }
  gfx::GpuMemoryBufferType GetType() override {
    return gmb_->GetHandle().type;
  }
  ClientBuffer AsClientBuffer() override { return gmb_->AsClientBuffer(); }
  base::PlatformFile AsPlatformFile() override {
    DCHECK_EQ(gmb_->GetHandle().type, gfx::SHARED_MEMORY_BUFFER);
#if defined(OS_POSIX)
    return gmb_->GetHandle().handle.fd;
#elif defined(OS_WIN)
    return gmb_->GetHandle().handle;
#endif
  }

 private:
  gfx::GpuMemoryBuffer* const gmb_;
  scoped_ptr<void*[]> data_;
  const size_t size_;
};

// Tracker specifics for SharedMemory.
class VideoCaptureBufferPool::SharedMemTracker final : public Tracker {
 public:
  SharedMemTracker();
  bool Init(VideoFrame::Format format,
            media::VideoFrame::StorageType storage_type,
            const gfx::Size& dimensions) override;

  size_t mapped_size() const override { return shared_memory_.mapped_size(); }

  scoped_ptr<BufferHandle> GetBufferHandle() override {
    return make_scoped_ptr(new SimpleBufferHandle(
        shared_memory_.memory(), mapped_size(), shared_memory_.handle()));
  }

  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    return shared_memory_.ShareToProcess(process_handle, new_handle);
  }

 private:
  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
};

// Tracker specifics for GpuMemoryBuffer. Owns one GpuMemoryBuffer and its
// associated pixel dimensions.
class VideoCaptureBufferPool::GpuMemoryBufferTracker final : public Tracker {
 public:
  GpuMemoryBufferTracker();
  bool Init(VideoFrame::Format format,
            media::VideoFrame::StorageType storage_type,
            const gfx::Size& dimensions) override;
  ~GpuMemoryBufferTracker() override;

  size_t mapped_size() const override { return packed_size_; }
  scoped_ptr<BufferHandle> GetBufferHandle() override {
    return make_scoped_ptr(new GpuMemoryBufferBufferHandle(
        gpu_memory_buffer_.get(), packed_size_));
  }

  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    return true;
  }

 private:
  size_t packed_size_;
  scoped_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
};

VideoCaptureBufferPool::SharedMemTracker::SharedMemTracker() : Tracker() {
}

bool VideoCaptureBufferPool::SharedMemTracker::Init(
    VideoFrame::Format format,
    media::VideoFrame::StorageType storage_type,
    const gfx::Size& dimensions) {
  DVLOG(2) << "allocating ShMem of " << dimensions.ToString();
  set_pixel_format(format);
  set_storage_type(storage_type);
  // Input |dimensions| can be 0x0 for trackers that do not require memory
  // backing. The allocated size is calculated using VideoFrame methods since
  // this will be the abstraction used to wrap the underlying data.
  set_pixel_count(dimensions.GetArea());
  const size_t byte_count = VideoFrame::AllocationSize(format, dimensions);
  if (!byte_count)
    return true;
  return shared_memory_.CreateAndMapAnonymous(byte_count);
}

VideoCaptureBufferPool::GpuMemoryBufferTracker::GpuMemoryBufferTracker()
    : Tracker(), gpu_memory_buffer_(nullptr) {}

VideoCaptureBufferPool::GpuMemoryBufferTracker::~GpuMemoryBufferTracker() {
  if (gpu_memory_buffer_->IsMapped())
    gpu_memory_buffer_->Unmap();
}

bool VideoCaptureBufferPool::GpuMemoryBufferTracker::Init(
    VideoFrame::Format format,
    media::VideoFrame::StorageType storage_type,
    const gfx::Size& dimensions) {
  DVLOG(2) << "allocating GMB for " << dimensions.ToString();
  // BrowserGpuMemoryBufferManager::current() may not be accessed on IO Thread.
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(BrowserGpuMemoryBufferManager::current());
  set_pixel_format(format);
  set_storage_type(storage_type);
  set_pixel_count(dimensions.GetArea());
  gpu_memory_buffer_ =
      BrowserGpuMemoryBufferManager::current()->AllocateGpuMemoryBuffer(
          dimensions,
          gfx::GpuMemoryBuffer::BGRA_8888,
          gfx::GpuMemoryBuffer::MAP);
  DLOG_IF(ERROR, !gpu_memory_buffer_.get()) << "Allocating GpuMemoryBuffer";
  if (!gpu_memory_buffer_.get())
    return false;
  int plane_sizes;
  gpu_memory_buffer_->GetStride(&plane_sizes);
  packed_size_ = plane_sizes * dimensions.height();
  return true;
}

//static
scoped_ptr<VideoCaptureBufferPool::Tracker>
VideoCaptureBufferPool::Tracker::CreateTracker(bool use_gmb) {
  if (!use_gmb)
    return make_scoped_ptr(new SharedMemTracker());
  else
    return make_scoped_ptr(new GpuMemoryBufferTracker());
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

base::SharedMemoryHandle VideoCaptureBufferPool::ShareToProcess(
    int buffer_id,
    base::ProcessHandle process_handle,
    size_t* memory_size) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return base::SharedMemory::NULLHandle();
  }
  base::SharedMemoryHandle remote_handle;
  if (tracker->ShareToProcess(process_handle, &remote_handle)) {
    *memory_size = tracker->mapped_size();
    return remote_handle;
  }
  DPLOG(ERROR) << "Error mapping Shared Memory";
  return base::SharedMemoryHandle();
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

int VideoCaptureBufferPool::ReserveForProducer(media::VideoPixelFormat format,
                                               const gfx::Size& dimensions,
                                               int* buffer_id_to_drop) {
  base::AutoLock lock(lock_);
  return ReserveForProducerInternal(format, dimensions, buffer_id_to_drop);
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

int VideoCaptureBufferPool::ReserveForProducerInternal(
    media::VideoPixelFormat format,
    const gfx::Size& dimensions,
    int* buffer_id_to_drop) {
  DCHECK(format == media::PIXEL_FORMAT_I420 ||
         format == media::PIXEL_FORMAT_TEXTURE ||
         format == media::PIXEL_FORMAT_GPUMEMORYBUFFER );
  lock_.AssertAcquired();
  *buffer_id_to_drop = kInvalidId;

  const size_t size_in_pixels = dimensions.GetArea();
  const media::VideoFrame::Format pixel_format =
      VideoPixelFormatToVideoFrameFormat(format);
  const media::VideoFrame::StorageType storage_type =
      VideoPixelFormatToVideoFrameStorageType(format);
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

  scoped_ptr<Tracker> tracker =
      Tracker::CreateTracker(format == media::PIXEL_FORMAT_GPUMEMORYBUFFER);
  if (!tracker->Init(pixel_format, storage_type, dimensions)) {
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
