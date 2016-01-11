// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_codecs.h"

#include "base/logging.h"

namespace media {

// The names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string GetCodecName(VideoCodec codec) {
  switch (codec) {
    case kUnknownVideoCodec:
      return "unknown";
    case kCodecH264:
      return "h264";
    case kCodecHEVC:
      return "hevc";
    case kCodecVC1:
      return "vc1";
    case kCodecMPEG2:
      return "mpeg2video";
    case kCodecMPEG4:
      return "mpeg4";
    case kCodecTheora:
      return "theora";
    case kCodecVP8:
      return "vp8";
    case kCodecVP9:
      return "vp9";
  }
  NOTREACHED();
  return "";
}

std::string GetProfileName(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return "unknown";
    case H264PROFILE_BASELINE:
      return "h264 baseline";
    case H264PROFILE_MAIN:
      return "h264 main";
    case H264PROFILE_EXTENDED:
      return "h264 extended";
    case H264PROFILE_HIGH:
      return "h264 high";
    case H264PROFILE_HIGH10PROFILE:
      return "h264 high 10";
    case H264PROFILE_HIGH422PROFILE:
      return "h264 high 4:2:2";
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
      return "h264 high 4:4:4 predictive";
    case H264PROFILE_SCALABLEBASELINE:
      return "h264 scalable baseline";
    case H264PROFILE_SCALABLEHIGH:
      return "h264 scalable high";
    case H264PROFILE_STEREOHIGH:
      return "h264 stereo high";
    case H264PROFILE_MULTIVIEWHIGH:
      return "h264 multiview high";
    case VP8PROFILE_ANY:
      return "vp8";
    case VP9PROFILE_ANY:
      return "vp9";
  }
  NOTREACHED();
  return "";
}

}  // namespace media
