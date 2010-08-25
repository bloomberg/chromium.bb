// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Borrowed from media/tools/omx_test/file_reader_util.cc.
// Added some functionalities related to timestamps on packets.

#include "media/mf/file_reader_util.h"

#include <cstring>

#include <algorithm>

#include "base/logging.h"
#include "media/base/data_buffer.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/bitstream_converter.h"

namespace media {

//////////////////////////////////////////////////////////////////////////////
// FFmpegFileReader
FFmpegFileReader::FFmpegFileReader(const std::string& filename)
    : filename_(filename),
      format_context_(NULL),
      codec_context_(NULL),
      target_stream_(-1),
      converter_(NULL),
      last_timestamp_(0) {
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

void FFmpegFileReader::Read(scoped_refptr<DataBuffer>* output) {
  if (!format_context_ || !codec_context_ || target_stream_ == -1) {
    *output = new DataBuffer(0);
    return;
  }
  AVPacket packet;
  bool found = false;
  while (!found) {
    int result = av_read_frame(format_context_, &packet);
    if (result < 0) {
      *output = new DataBuffer(0);
      return;
    }
    if (packet.stream_index == target_stream_) {
      if (converter_.get() && !converter_->ConvertPacket(&packet)) {
        LOG(ERROR) << "failed to convert AVPacket";
      }
      last_timestamp_ = std::max(last_timestamp_, packet.pts);
      CopyPacketToBuffer(&packet, output);
      found = true;
    }
    av_free_packet(&packet);
  }
}

bool FFmpegFileReader::SeekForward(int64 seek_amount_us) {
  if (!format_context_ || !codec_context_ || target_stream_ == -1) {
    return false;
  }
  int64 new_us = TimeBaseToMicroseconds(last_timestamp_) + seek_amount_us;
  int64 new_timestamp = MicrosecondsToTimeBase(new_us);
  last_timestamp_ = new_timestamp;
  return av_seek_frame(format_context_, target_stream_, new_timestamp, 0) >= 0;
}

bool FFmpegFileReader::GetFrameRate(int* num, int* denom) const {
  if (!codec_context_)
    return false;
  *denom = codec_context_->time_base.num;
  *num = codec_context_->time_base.den;
  if (*denom == 0) {
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

int64 FFmpegFileReader::TimeBaseToMicroseconds(
    int64 time_base_unit) const {
  // FFmpeg units after time base conversion seems to be actually given in
  // milliseconds (instead of seconds...) so we need to multiply it by a factor
  // of 1,000.
  // Note we need to double this because the frame rate is doubled in
  // ffmpeg.
  CHECK(codec_context_) << "Codec context needs to be initialized";
  return time_base_unit * 2000 * codec_context_->time_base.num /
         codec_context_->time_base.den;
}

int64 FFmpegFileReader::MicrosecondsToTimeBase(
    int64 time_base_unit) const {
  CHECK(codec_context_) << "Codec context needs to be initialized";
  return time_base_unit * codec_context_->time_base.den / 2000 /
         codec_context_->time_base.num;
}

void FFmpegFileReader::CopyPacketToBuffer(AVPacket* packet,
                                          scoped_refptr<DataBuffer>* output) {
  uint8* buffer = new uint8[packet->size];
  if (buffer == NULL) {
    LOG(ERROR) << "Failed to allocate buffer for annex b stream";
    *output = NULL;
    return;
  }
  memcpy(buffer, packet->data, packet->size);
  *output = new DataBuffer(buffer, packet->size);
  if (packet->pts != AV_NOPTS_VALUE) {
    (*output)->SetTimestamp(
        base::TimeDelta::FromMicroseconds(
            TimeBaseToMicroseconds(packet->pts)));
  } else {
    (*output)->SetTimestamp(StreamSample::kInvalidTimestamp);
  }
  if (packet->duration == 0) {
    LOG(WARNING) << "Packet duration not known";
  }
  (*output)->SetDuration(
      base::TimeDelta::FromMicroseconds(
          TimeBaseToMicroseconds(packet->duration)));
}

}  // namespace media
