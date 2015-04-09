// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_buffer_pool.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"

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
      {media::PIXEL_FORMAT_ARGB, VideoFrame::ARGB},
      {media::PIXEL_FORMAT_TEXTURE, VideoFrame::NATIVE_TEXTURE},
  };

  for (const auto& format_pair : kVideoPixelFormatToVideoFrameFormat) {
    if (format_pair.pixel_format == pixel_format)
      return format_pair.frame_format;
  }
  LOG(ERROR) << "Unsupported VideoPixelFormat "
             << media::VideoCaptureFormat::PixelFormatToString(pixel_format);
  return VideoFrame::UNKNOWN;
}

// Tracker specifics for SharedMemory.
class VideoCaptureBufferPool::SharedMemTracker final : public Tracker {
 public:
  SharedMemTracker();

  bool Init(VideoFrame::Format format, const gfx::Size& dimensions) override;
  void* storage() override { return shared_memory_.memory(); }
  size_t requested_size() override { return shared_memory_.requested_size(); }
  size_t mapped_size() override { return shared_memory_.mapped_size(); }

  bool ShareToProcess(base::ProcessHandle process_handle,
                      base::SharedMemoryHandle* new_handle) override {
    return shared_memory_.ShareToProcess(process_handle, new_handle);
  }

 private:
  // The memory created to be shared with renderer processes.
  base::SharedMemory shared_memory_;
};

VideoCaptureBufferPool::SharedMemTracker::SharedMemTracker()
    : Tracker() {}

bool VideoCaptureBufferPool::SharedMemTracker::Init(
    VideoFrame::Format format,
    const gfx::Size& dimensions) {
  // Input |dimensions| can be 0x0 for trackers that do not require memory
  // backing. The allocated size is calculated using VideoFrame methods since
  // this will be the abstraction used to wrap the underlying data.
  return shared_memory_.CreateAndMapAnonymous(
      VideoFrame::AllocationSize(format, dimensions));
}

//static
scoped_ptr<VideoCaptureBufferPool::Tracker>
VideoCaptureBufferPool::Tracker::CreateTracker() {
  return make_scoped_ptr(new SharedMemTracker());
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
  DPLOG(ERROR) << "Error mapping Shared Memory.";
  return base::SharedMemoryHandle();
}

bool VideoCaptureBufferPool::GetBufferInfo(int buffer_id,
                                           void** storage,
                                           size_t* size) {
  base::AutoLock lock(lock_);

  Tracker* tracker = GetTracker(buffer_id);
  if (!tracker) {
    NOTREACHED() << "Invalid buffer_id.";
    return false;
  }

  DCHECK(tracker->held_by_producer());
  *storage = tracker->storage();
  *size = tracker->mapped_size();
  return true;
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
         format == media::PIXEL_FORMAT_ARGB ||
         format == media::PIXEL_FORMAT_TEXTURE);
  lock_.AssertAcquired();
  const media::VideoFrame::Format frame_format =
      VideoPixelFormatToVideoFrameFormat(format);
  const size_t size_in_bytes =
      VideoFrame::AllocationSize(frame_format, dimensions);

  // Look for a tracker that's allocated, big enough, and not in use. Track the
  // largest one that's not big enough, in case we have to reallocate a tracker.
  *buffer_id_to_drop = kInvalidId;
  size_t realloc_size = 0;
  TrackerMap::iterator tracker_to_drop = trackers_.end();
  for (TrackerMap::iterator it = trackers_.begin(); it != trackers_.end();
       ++it) {
    Tracker* const tracker = it->second;
    if (!tracker->consumer_hold_count() && !tracker->held_by_producer()) {
      if (tracker->requested_size() >= size_in_bytes) {
        // Existing tracker is big enough. Reuse it.
        tracker->set_held_by_producer(true);
        return it->first;
      }
      if (tracker->requested_size() > realloc_size) {
        realloc_size = tracker->requested_size();
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
  scoped_ptr<Tracker> tracker = Tracker::CreateTracker();
  if (!tracker->Init(frame_format, dimensions))
    return kInvalidId;
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
