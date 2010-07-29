// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Borrowed from media/tools/omx_test/file_reader_util.cc.
// Added some functionalities related to timestamps on packets.

#include "media/mf/file_reader_util.h"

#include <algorithm>

#include "base/scoped_comptr_win.h"
#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/bitstream_converter.h"
#include "media/mf/h264mft.h"

namespace media {

//////////////////////////////////////////////////////////////////////////////
// FFmpegFileReader
FFmpegFileReader::FFmpegFileReader(const std::string& filename)
    : filename_(filename),
      format_context_(NULL),
      codec_context_(NULL),
      target_stream_(-1),
      converter_(NULL),
      end_of_stream_(false) {
}

FFmpegFileReader::~FFmpegFileReader() {
  if (format_context_)
    av_close_input_file(format_context_);
}

bool FFmpegFileReader::Initialize() {
  int result = av_open_input_file(&format_context_, filename_.c_str(),
                                  NULL, 0, NULL);
  if (result < 0) {
    switch (result) {
      case AVERROR_NOFMT:
        LOG(ERROR) << "Error: File format not supported "
                  << filename_;
        break;
      default:
        LOG(ERROR) << "Error: Could not open input for "
                   << filename_ << ": " << result;
        break;
    }
    return false;
  }
  if (av_find_stream_info(format_context_) < 0) {
    LOG(ERROR) << "can't use FFmpeg to parse stream info";
    return false;
  }

  for (size_t i = 0; i < format_context_->nb_streams; ++i) {
    codec_context_ = format_context_->streams[i]->codec;

    // Find the video stream.
    if (codec_context_->codec_type == CODEC_TYPE_VIDEO) {
      target_stream_ = i;
      break;
    }
  }
  if (target_stream_ == -1) {
    LOG(ERROR) << "no video in the stream";
    return false;
  }

  // Initialize the bitstream filter if needed.
  // TODO(hclam): find a better way to identify mp4 container.
  if (codec_context_->codec_id == CODEC_ID_H264) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "h264_mp4toannexb", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_MPEG4) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "mpeg4video_es", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_WMV3) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "vc1_asftorcv", codec_context_));
  } else if (codec_context_->codec_id == CODEC_ID_VC1) {
    converter_.reset(new media::FFmpegBitstreamConverter(
        "vc1_asftoannexg", codec_context_));
  }
  if (converter_.get() && !converter_->Initialize()) {
    converter_.reset();
    LOG(ERROR) << "failed to initialize h264_mp4toannexb filter";
    return false;
  }
  return true;
}

void FFmpegFileReader::Read(uint8** output, int* size) {
  Read(output, size, NULL, NULL);
}

void FFmpegFileReader::Read(uint8** output, int* size, int* duration,
                            int64* sample_time) {
  if (!format_context_ || !codec_context_ || target_stream_ == -1) {
    *size = 0;
    *output = NULL;
    return;
  }
  AVPacket packet;
  bool found = false;
  while (!found) {
    int result = av_read_frame(format_context_, &packet);
    if (result < 0) {
      *output = NULL;
      *size = 0;
      end_of_stream_ = true;
      return;
    }
    if (packet.stream_index == target_stream_) {
      if (converter_.get() && !converter_->ConvertPacket(&packet)) {
        LOG(ERROR) << "failed to convert AVPacket";
      }
      *output = new uint8[packet.size];
      if (*output == NULL) {
        LOG(ERROR) << "Failed to allocate buffer for annex b stream";
        *size = 0;
        return;
      }
      *size = packet.size;
      memcpy(*output, packet.data, packet.size);
      if (duration) {
        if (packet.duration == 0) {
          LOG(WARNING) << "Packet duration not known";
        }
        // This is in AVCodecContext::time_base units
        *duration = packet.duration;
      }
      if (sample_time) {
        if (packet.pts == AV_NOPTS_VALUE) {
          LOG(ERROR) << "Packet presentation time not known";
          *sample_time = 0L;
        } else {
          // This is in AVCodecContext::time_base units
          *sample_time = packet.pts;
        }
      }
      found = true;
    }
    av_free_packet(&packet);
  }
}

bool FFmpegFileReader::GetFrameRate(int* num, int *denom) const {
  if (!codec_context_)
    return false;
  *num = codec_context_->time_base.num;
  *denom = codec_context_->time_base.den;
  if (denom == 0) {
    *num = 0;
    return false;
  }
  return true;
}

bool FFmpegFileReader::GetWidth(int* width) const {
  if (!codec_context_)
    return false;
  *width = codec_context_->width;
  return true;
}

bool FFmpegFileReader::GetHeight(int* height) const {
  if (!codec_context_)
    return false;
  *height = codec_context_->height;
  return true;
}

bool FFmpegFileReader::GetAspectRatio(int* num, int* denom) const {
  if (!codec_context_)
    return false;
  AVRational aspect_ratio = codec_context_->sample_aspect_ratio;
  if (aspect_ratio.num == 0 || aspect_ratio.den == 0)
    return false;
  *num = aspect_ratio.num;
  *denom = aspect_ratio.den;
  return true;
}

int64 FFmpegFileReader::ConvertFFmpegTimeBaseTo100Ns(
    int64 time_base_unit) const {
  // FFmpeg units after time base conversion seems to be actually given in
  // milliseconds (instead of seconds...) so we need to multiply it by a factor
  // of 10,000 to convert it into units compatible with MF.
  CHECK(codec_context_) << "Codec context needs to be initialized";
  return time_base_unit * 10000 * codec_context_->time_base.num /
         codec_context_->time_base.den;
}

}  // namespace media
