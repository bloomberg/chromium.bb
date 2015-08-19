// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_capture_types.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/base/limits.h"

namespace media {

VideoCaptureFormat::VideoCaptureFormat()
    : frame_rate(0.0f),
      pixel_format(VIDEO_CAPTURE_PIXEL_FORMAT_UNKNOWN),
      pixel_storage(PIXEL_STORAGE_CPU) {
}

VideoCaptureFormat::VideoCaptureFormat(const gfx::Size& frame_size,
                                       float frame_rate,
                                       VideoCapturePixelFormat pixel_format)
    : frame_size(frame_size),
      frame_rate(frame_rate),
      pixel_format(pixel_format),
      pixel_storage(PIXEL_STORAGE_CPU) {
}

VideoCaptureFormat::VideoCaptureFormat(const gfx::Size& frame_size,
                                       float frame_rate,
                                       VideoCapturePixelFormat pixel_format,
                                       VideoPixelStorage pixel_storage)
    : frame_size(frame_size),
      frame_rate(frame_rate),
      pixel_format(pixel_format),
      pixel_storage(pixel_storage) {
}

bool VideoCaptureFormat::IsValid() const {
  return (frame_size.width() < media::limits::kMaxDimension) &&
         (frame_size.height() < media::limits::kMaxDimension) &&
         (frame_size.GetArea() >= 0) &&
         (frame_size.GetArea() < media::limits::kMaxCanvas) &&
         (frame_rate >= 0.0f) &&
         (frame_rate < media::limits::kMaxFramesPerSecond) &&
         (pixel_storage != PIXEL_STORAGE_TEXTURE ||
          pixel_format == VIDEO_CAPTURE_PIXEL_FORMAT_ARGB);
}

size_t VideoCaptureFormat::ImageAllocationSize() const {
  size_t result_frame_size = frame_size.GetArea();
  switch (pixel_format) {
    case VIDEO_CAPTURE_PIXEL_FORMAT_I420:
    case VIDEO_CAPTURE_PIXEL_FORMAT_YV12:
    case VIDEO_CAPTURE_PIXEL_FORMAT_NV12:
    case VIDEO_CAPTURE_PIXEL_FORMAT_NV21:
      result_frame_size = result_frame_size * 3 / 2;
      break;
    case VIDEO_CAPTURE_PIXEL_FORMAT_UYVY:
    case VIDEO_CAPTURE_PIXEL_FORMAT_YUY2:
      result_frame_size *= 2;
      break;
    case VIDEO_CAPTURE_PIXEL_FORMAT_RGB24:
      result_frame_size *= 3;
      break;
    case VIDEO_CAPTURE_PIXEL_FORMAT_RGB32:
    case VIDEO_CAPTURE_PIXEL_FORMAT_ARGB:
      result_frame_size *= 4;
      break;
    case VIDEO_CAPTURE_PIXEL_FORMAT_MJPEG:
      result_frame_size = 0;
      break;
    default:  // Sizes for the rest of the formats are unknown.
      NOTREACHED() << "Unknown pixel format provided.";
      break;
  }
  return result_frame_size;
}

//static
std::string VideoCaptureFormat::ToString(const VideoCaptureFormat& format) {
  return base::StringPrintf("(%s)@%.3ffps, pixel format: %s storage: %s.",
                            format.frame_size.ToString().c_str(),
                            format.frame_rate,
                            PixelFormatToString(format.pixel_format).c_str(),
                            PixelStorageToString(format.pixel_storage).c_str());
}

// static
std::string VideoCaptureFormat::PixelFormatToString(
    VideoCapturePixelFormat format) {
  switch (format) {
    case VIDEO_CAPTURE_PIXEL_FORMAT_UNKNOWN:
      return "UNKNOWN";
    case VIDEO_CAPTURE_PIXEL_FORMAT_I420:
      return "I420";
    case VIDEO_CAPTURE_PIXEL_FORMAT_YUY2:
      return "YUY2";
    case VIDEO_CAPTURE_PIXEL_FORMAT_UYVY:
      return "UYVY";
    case VIDEO_CAPTURE_PIXEL_FORMAT_RGB24:
      return "RGB24";
    case VIDEO_CAPTURE_PIXEL_FORMAT_RGB32:
      return "RGB32";
    case VIDEO_CAPTURE_PIXEL_FORMAT_ARGB:
      return "ARGB";
    case VIDEO_CAPTURE_PIXEL_FORMAT_MJPEG:
      return "MJPEG";
    case VIDEO_CAPTURE_PIXEL_FORMAT_NV12:
      return "NV12";
    case VIDEO_CAPTURE_PIXEL_FORMAT_NV21:
      return "NV21";
    case VIDEO_CAPTURE_PIXEL_FORMAT_YV12:
      return "YV12";
  }
  NOTREACHED() << "Invalid VideoCapturePixelFormat provided: " << format;
  return std::string();
}

// static
std::string VideoCaptureFormat::PixelStorageToString(
    VideoPixelStorage storage) {
  switch (storage) {
    case PIXEL_STORAGE_CPU:
      return "CPU";
    case PIXEL_STORAGE_TEXTURE:
      return "TEXTURE";
    case PIXEL_STORAGE_GPUMEMORYBUFFER:
      return "GPUMEMORYBUFFER";
  }
  NOTREACHED() << "Invalid VideoPixelStorage provided: "
               << static_cast<int>(storage);
  return std::string();
}

VideoCaptureParams::VideoCaptureParams()
    : resolution_change_policy(RESOLUTION_POLICY_FIXED_RESOLUTION) {
}

}  // namespace media
