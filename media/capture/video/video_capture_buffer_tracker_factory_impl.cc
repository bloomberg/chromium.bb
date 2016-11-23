// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"

#include "base/memory/ptr_util.h"

#include "media/capture/video/shared_memory_buffer_tracker.h"

namespace media {

std::unique_ptr<VideoCaptureBufferTracker>
VideoCaptureBufferTrackerFactoryImpl::CreateTracker(VideoPixelStorage storage) {
  DCHECK_EQ(PIXEL_STORAGE_CPU, storage);
  return base::MakeUnique<SharedMemoryBufferTracker>();
}

}  // namespace media
