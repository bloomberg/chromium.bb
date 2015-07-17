// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/linux/v4l2_capture_delegate_single_plane.h"

#include <sys/mman.h>

namespace media {

scoped_refptr<V4L2CaptureDelegate::BufferTracker>
V4L2CaptureDelegateSinglePlane::CreateBufferTracker() const {
  return make_scoped_refptr(new BufferTrackerSPlane());
}

bool V4L2CaptureDelegateSinglePlane::FillV4L2Format(
    v4l2_format* format,
    uint32_t width,
    uint32_t height,
    uint32_t pixelformat_fourcc) const {
  format->fmt.pix.width = width;
  format->fmt.pix.height = height;
  format->fmt.pix.pixelformat = pixelformat_fourcc;
  return true;
}

void V4L2CaptureDelegateSinglePlane::FinishFillingV4L2Buffer(
    v4l2_buffer* buffer) const {
  buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
}

void V4L2CaptureDelegateSinglePlane::SetPayloadSize(
    const scoped_refptr<BufferTracker>& buffer_tracker,
    const v4l2_buffer& buffer) const {
  buffer_tracker->SetPlanePayloadSize(0, buffer.bytesused);
}

void V4L2CaptureDelegateSinglePlane::SendBuffer(
    const scoped_refptr<BufferTracker>& buffer_tracker,
    const v4l2_format& format) const {
  client()->OnIncomingCapturedData(
      buffer_tracker->GetPlaneStart(0), buffer_tracker->GetPlanePayloadSize(0),
      capture_format(), rotation(), base::TimeTicks::Now());
}

bool V4L2CaptureDelegateSinglePlane::BufferTrackerSPlane::Init(
    int fd,
    const v4l2_buffer& buffer) {
  // Some devices require mmap() to be called with both READ and WRITE.
  // See http://crbug.com/178582.
  void* const start = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, buffer.m.offset);
  if (start == MAP_FAILED) {
    DLOG(ERROR) << "Error mmap()ing a V4L2 buffer into userspace";
    return false;
  }
  AddMmapedPlane(static_cast<uint8_t*>(start), buffer.length);
  return true;
}

}  // namespace media
