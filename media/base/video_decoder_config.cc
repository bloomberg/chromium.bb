// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_decoder_config.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "media/base/video_frame.h"

namespace media {

VideoCodec VideoCodecProfileToVideoCodec(VideoCodecProfile profile) {
  switch (profile) {
    case VIDEO_CODEC_PROFILE_UNKNOWN:
      return kUnknownVideoCodec;
    case H264PROFILE_BASELINE:
    case H264PROFILE_MAIN:
    case H264PROFILE_EXTENDED:
    case H264PROFILE_HIGH:
    case H264PROFILE_HIGH10PROFILE:
    case H264PROFILE_HIGH422PROFILE:
    case H264PROFILE_HIGH444PREDICTIVEPROFILE:
    case H264PROFILE_SCALABLEBASELINE:
    case H264PROFILE_SCALABLEHIGH:
    case H264PROFILE_STEREOHIGH:
    case H264PROFILE_MULTIVIEWHIGH:
      return kCodecH264;
    case VP8PROFILE_ANY:
      return kCodecVP8;
    case VP9PROFILE_ANY:
      return kCodecVP9;
  }
  NOTREACHED();
  return kUnknownVideoCodec;
}


VideoDecoderConfig::VideoDecoderConfig()
    : codec_(kUnknownVideoCodec),
      profile_(VIDEO_CODEC_PROFILE_UNKNOWN),
      format_(PIXEL_FORMAT_UNKNOWN),
      is_encrypted_(false) {
}

VideoDecoderConfig::VideoDecoderConfig(VideoCodec codec,
                                       VideoCodecProfile profile,
                                       VideoPixelFormat format,
                                       ColorSpace color_space,
                                       const gfx::Size& coded_size,
                                       const gfx::Rect& visible_rect,
                                       const gfx::Size& natural_size,
                                       const uint8* extra_data,
                                       size_t extra_data_size,
                                       bool is_encrypted) {
  Initialize(codec, profile, format, color_space, coded_size, visible_rect,
             natural_size, extra_data, extra_data_size, is_encrypted, true);
}

VideoDecoderConfig::~VideoDecoderConfig() {}

// Some videos just want to watch the world burn, with a height of 0; cap the
// "infinite" aspect ratio resulting.
static const int kInfiniteRatio = 99999;

// Common aspect ratios (multiplied by 100 and truncated) used for histogramming
// video sizes.  These were taken on 20111103 from
// http://wikipedia.org/wiki/Aspect_ratio_(image)#Previous_and_currently_used_aspect_ratios
static const int kCommonAspectRatios100[] = {
  100, 115, 133, 137, 143, 150, 155, 160, 166, 175, 177, 185, 200, 210, 220,
  221, 235, 237, 240, 255, 259, 266, 276, 293, 400, 1200, kInfiniteRatio,
};

template<class T>  // T has int width() & height() methods.
static void UmaHistogramAspectRatio(const char* name, const T& size) {
  UMA_HISTOGRAM_CUSTOM_ENUMERATION(
      name,
      // Intentionally use integer division to truncate the result.
      size.height() ? (size.width() * 100) / size.height() : kInfiniteRatio,
      base::CustomHistogram::ArrayToCustomRanges(
          kCommonAspectRatios100, arraysize(kCommonAspectRatios100)));
}

void VideoDecoderConfig::Initialize(VideoCodec codec,
                                    VideoCodecProfile profile,
                                    VideoPixelFormat format,
                                    ColorSpace color_space,
                                    const gfx::Size& coded_size,
                                    const gfx::Rect& visible_rect,
                                    const gfx::Size& natural_size,
                                    const uint8* extra_data,
                                    size_t extra_data_size,
                                    bool is_encrypted,
                                    bool record_stats) {
  CHECK((extra_data_size != 0) == (extra_data != NULL));

  if (record_stats) {
    UMA_HISTOGRAM_ENUMERATION("Media.VideoCodec", codec, kVideoCodecMax + 1);
    // Drop UNKNOWN because U_H_E() uses one bucket for all values less than 1.
    if (profile >= 0) {
      UMA_HISTOGRAM_ENUMERATION("Media.VideoCodecProfile", profile,
                                VIDEO_CODEC_PROFILE_MAX + 1);
    }
    UMA_HISTOGRAM_COUNTS_10000("Media.VideoCodedWidth", coded_size.width());
    UmaHistogramAspectRatio("Media.VideoCodedAspectRatio", coded_size);
    UMA_HISTOGRAM_COUNTS_10000("Media.VideoVisibleWidth", visible_rect.width());
    UmaHistogramAspectRatio("Media.VideoVisibleAspectRatio", visible_rect);
    UMA_HISTOGRAM_ENUMERATION("Media.VideoFramePixelFormat", format,
                              PIXEL_FORMAT_MAX + 1);
    UMA_HISTOGRAM_ENUMERATION("Media.VideoFrameColorSpace", color_space,
                              COLOR_SPACE_MAX + 1);
  }

  codec_ = codec;
  profile_ = profile;
  format_ = format;
  color_space_ = color_space;
  coded_size_ = coded_size;
  visible_rect_ = visible_rect;
  natural_size_ = natural_size;
  extra_data_.assign(extra_data, extra_data + extra_data_size);
  is_encrypted_ = is_encrypted;
}

bool VideoDecoderConfig::IsValidConfig() const {
  return codec_ != kUnknownVideoCodec &&
      natural_size_.width() > 0 &&
      natural_size_.height() > 0 &&
      VideoFrame::IsValidConfig(format_, VideoFrame::STORAGE_UNOWNED_MEMORY,
                                coded_size_, visible_rect_, natural_size_);
}

bool VideoDecoderConfig::Matches(const VideoDecoderConfig& config) const {
  return ((codec() == config.codec()) &&
          (format() == config.format()) &&
          (profile() == config.profile()) &&
          (coded_size() == config.coded_size()) &&
          (visible_rect() == config.visible_rect()) &&
          (natural_size() == config.natural_size()) &&
          (extra_data_size() == config.extra_data_size()) &&
          (!extra_data() || !memcmp(extra_data(), config.extra_data(),
                                    extra_data_size())) &&
          (is_encrypted() == config.is_encrypted()));
}

std::string VideoDecoderConfig::AsHumanReadableString() const {
  std::ostringstream s;
  s << "codec: " << GetHumanReadableCodecName()
    << " format: " << format()
    << " profile: " << profile()
    << " coded size: [" << coded_size().width()
    << "," << coded_size().height() << "]"
    << " visible rect: [" << visible_rect().x()
    << "," << visible_rect().y()
    << "," << visible_rect().width()
    << "," << visible_rect().height() << "]"
    << " natural size: [" << natural_size().width()
    << "," << natural_size().height() << "]"
    << " has extra data? " << (extra_data() ? "true" : "false")
    << " encrypted? " << (is_encrypted() ? "true" : "false");
  return s.str();
}

// The names come from src/third_party/ffmpeg/libavcodec/codec_desc.c
std::string VideoDecoderConfig::GetHumanReadableCodecName() const {
  switch (codec()) {
    case kUnknownVideoCodec:
      return "unknown";
    case kCodecH264:
      return "h264";
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

const uint8* VideoDecoderConfig::extra_data() const {
  if (extra_data_.empty())
    return NULL;
  return &extra_data_[0];
}

}  // namespace media
