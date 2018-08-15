// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_types.h"

#include "base/logging.h"

namespace media {

std::string VideoPixelFormatToString(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
      return "PIXEL_FORMAT_UNKNOWN";
    case PIXEL_FORMAT_I420:
      return "PIXEL_FORMAT_I420";
    case PIXEL_FORMAT_YV12:
      return "PIXEL_FORMAT_YV12";
    case PIXEL_FORMAT_I422:
      return "PIXEL_FORMAT_I422";
    case PIXEL_FORMAT_I420A:
      return "PIXEL_FORMAT_I420A";
    case PIXEL_FORMAT_I444:
      return "PIXEL_FORMAT_I444";
    case PIXEL_FORMAT_NV12:
      return "PIXEL_FORMAT_NV12";
    case PIXEL_FORMAT_NV21:
      return "PIXEL_FORMAT_NV21";
    case PIXEL_FORMAT_UYVY:
      return "PIXEL_FORMAT_UYVY";
    case PIXEL_FORMAT_YUY2:
      return "PIXEL_FORMAT_YUY2";
    case PIXEL_FORMAT_ARGB:
      return "PIXEL_FORMAT_ARGB";
    case PIXEL_FORMAT_XRGB:
      return "PIXEL_FORMAT_XRGB";
    case PIXEL_FORMAT_RGB24:
      return "PIXEL_FORMAT_RGB24";
    case PIXEL_FORMAT_RGB32:
      return "PIXEL_FORMAT_RGB32";
    case PIXEL_FORMAT_MJPEG:
      return "PIXEL_FORMAT_MJPEG";
    case PIXEL_FORMAT_MT21:
      return "PIXEL_FORMAT_MT21";
    case PIXEL_FORMAT_YUV420P9:
      return "PIXEL_FORMAT_YUV420P9";
    case PIXEL_FORMAT_YUV420P10:
      return "PIXEL_FORMAT_YUV420P10";
    case PIXEL_FORMAT_YUV422P9:
      return "PIXEL_FORMAT_YUV422P9";
    case PIXEL_FORMAT_YUV422P10:
      return "PIXEL_FORMAT_YUV422P10";
    case PIXEL_FORMAT_YUV444P9:
      return "PIXEL_FORMAT_YUV444P9";
    case PIXEL_FORMAT_YUV444P10:
      return "PIXEL_FORMAT_YUV444P10";
    case PIXEL_FORMAT_YUV420P12:
      return "PIXEL_FORMAT_YUV420P12";
    case PIXEL_FORMAT_YUV422P12:
      return "PIXEL_FORMAT_YUV422P12";
    case PIXEL_FORMAT_YUV444P12:
      return "PIXEL_FORMAT_YUV444P12";
    case PIXEL_FORMAT_Y16:
      return "PIXEL_FORMAT_Y16";
  }
  NOTREACHED() << "Invalid VideoPixelFormat provided: " << format;
  return "";
}

bool IsYuvPlanar(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_MT21:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      return true;

    case PIXEL_FORMAT_UNKNOWN:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_Y16:
      return false;
  }
  return false;
}

bool IsOpaque(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_MT21:
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P9:
    case PIXEL_FORMAT_YUV444P10:
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
    case PIXEL_FORMAT_Y16:
      return true;
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_RGB32:
      break;
  }
  return false;
}

size_t BitDepth(VideoPixelFormat format) {
  switch (format) {
    case PIXEL_FORMAT_UNKNOWN:
      NOTREACHED();
      FALLTHROUGH;
    case PIXEL_FORMAT_I420:
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I422:
    case PIXEL_FORMAT_I420A:
    case PIXEL_FORMAT_I444:
    case PIXEL_FORMAT_NV12:
    case PIXEL_FORMAT_NV21:
    case PIXEL_FORMAT_UYVY:
    case PIXEL_FORMAT_YUY2:
    case PIXEL_FORMAT_ARGB:
    case PIXEL_FORMAT_XRGB:
    case PIXEL_FORMAT_RGB24:
    case PIXEL_FORMAT_RGB32:
    case PIXEL_FORMAT_MJPEG:
    case PIXEL_FORMAT_MT21:
      return 8;
    case PIXEL_FORMAT_YUV420P9:
    case PIXEL_FORMAT_YUV422P9:
    case PIXEL_FORMAT_YUV444P9:
      return 9;
    case PIXEL_FORMAT_YUV420P10:
    case PIXEL_FORMAT_YUV422P10:
    case PIXEL_FORMAT_YUV444P10:
      return 10;
    case PIXEL_FORMAT_YUV420P12:
    case PIXEL_FORMAT_YUV422P12:
    case PIXEL_FORMAT_YUV444P12:
      return 12;
    case PIXEL_FORMAT_Y16:
      return 16;
  }
  NOTREACHED();
  return 0;
}

}  // namespace media
