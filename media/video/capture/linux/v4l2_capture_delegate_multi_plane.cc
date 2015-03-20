// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/v4l2_capture_delegate_multi_plane.h"

#include <sys/mman.h>

namespace media {

V4L2CaptureDelegateMultiPlane::V4L2CaptureDelegateMultiPlane(
    const VideoCaptureDevice::Name& device_name,
    const scoped_refptr<base::SingleThreadTaskRunner>& v4l2_task_runner,
    int power_line_frequency)
    : V4L2CaptureDelegate(device_name,
                          v4l2_task_runner,
                          power_line_frequency) {
}

V4L2CaptureDelegateMultiPlane::~V4L2CaptureDelegateMultiPlane() {
}

scoped_refptr<V4L2CaptureDelegate::BufferTracker>
V4L2CaptureDelegateMultiPlane::CreateBufferTracker() const {
  return make_scoped_refptr(new BufferTrackerMPlane());
}

bool V4L2CaptureDelegateMultiPlane::FillV4L2Format(
    v4l2_format* format,
    uint32_t width,
    uint32_t height,
    uint32_t pixelformat_fourcc) const {
  format->fmt.pix_mp.width = width;
  format->fmt.pix_mp.height = height;
  format->fmt.pix_mp.pixelformat = pixelformat_fourcc;

  const size_t num_v4l2_planes =
      V4L2CaptureDelegate::GetNumPlanesForFourCc(pixelformat_fourcc);
  if (num_v4l2_planes == 0u)
    return false;
  DCHECK_LE(num_v4l2_planes, static_cast<size_t>(VIDEO_MAX_PLANES));
  format->fmt.pix_mp.num_planes = num_v4l2_planes;

  v4l2_planes_.resize(num_v4l2_planes);
  return true;
}

void V4L2CaptureDelegateMultiPlane::FinishFillingV4L2Buffer(
    v4l2_buffer* buffer) const {
  buffer->type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  buffer->length = v4l2_planes_.size();

  static const struct v4l2_plane empty_plane = {};
  std::fill(v4l2_planes_.begin(), v4l2_planes_.end(), empty_plane);
  buffer->m.planes = v4l2_planes_.data();
}

void V4L2CaptureDelegateMultiPlane::SendBuffer(
    const scoped_refptr<BufferTracker>& buffer_tracker,
    const v4l2_format& format) const {
  DCHECK_EQ(capture_format().pixel_format, PIXEL_FORMAT_I420);
  const size_t y_stride = format.fmt.pix_mp.plane_fmt[0].bytesperline;
  const size_t u_stride = format.fmt.pix_mp.plane_fmt[1].bytesperline;
  const size_t v_stride = format.fmt.pix_mp.plane_fmt[2].bytesperline;
  DCHECK_GE(y_stride, 1u * capture_format().frame_size.width());
  DCHECK_GE(u_stride, 1u * capture_format().frame_size.width() / 2);
  DCHECK_GE(v_stride, 1u * capture_format().frame_size.width() / 2);
  client()->OnIncomingCapturedYuvData(buffer_tracker->GetPlaneStart(0),
                                      buffer_tracker->GetPlaneStart(1),
                                      buffer_tracker->GetPlaneStart(2),
                                      y_stride,
                                      u_stride,
                                      v_stride,
                                      capture_format(),
                                      rotation(),
                                      base::TimeTicks::Now());
}

bool V4L2CaptureDelegateMultiPlane::BufferTrackerMPlane::Init(
    int fd,
    const v4l2_buffer& buffer) {
  for (size_t p = 0; p < buffer.length; ++p) {
    // Some devices require mmap() to be called with both READ and WRITE.
    // See http://crbug.com/178582.
    void* const start =
        mmap(NULL, buffer.m.planes[p].length, PROT_READ | PROT_WRITE,
             MAP_SHARED, fd, buffer.m.planes[p].m.mem_offset);
    if (start == MAP_FAILED) {
      DLOG(ERROR) << "Error mmap()ing a V4L2 buffer into userspace";
      return false;
    }
    AddMmapedPlane(static_cast<uint8_t*>(start), buffer.m.planes[p].length);
    DVLOG(3) << "Mmap()ed plane #" << p << " of " << buffer.m.planes[p].length
             << "B";
  }
  return true;
}

}  // namespace media
