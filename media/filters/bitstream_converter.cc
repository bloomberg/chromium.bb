// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/bitstream_converter.h"

#include "media/filters/ffmpeg_common.h"

namespace media {

FFmpegBitstreamConverter::FFmpegBitstreamConverter(
    const std::string& filter_name,
    AVCodecContext* stream_context)
    : filter_name_(filter_name),
      stream_filter_(NULL),
      stream_context_(stream_context) {
  CHECK(stream_context_);
}

FFmpegBitstreamConverter::~FFmpegBitstreamConverter() {
  if (stream_filter_) {
    av_bitstream_filter_close(stream_filter_);
    stream_filter_ = NULL;
  }
}

bool FFmpegBitstreamConverter::Initialize() {
  stream_filter_ = av_bitstream_filter_init(filter_name_.c_str());
  return stream_filter_ != NULL;
}

bool FFmpegBitstreamConverter::ConvertPacket(AVPacket* packet) {
  CHECK(packet);

  if (!stream_filter_) {
    LOG(ERROR) << "Converter improperly initialized.";
    return false;
  }

  uint8_t* converted_data = NULL;
  int converted_size = 0;

  if (av_bitstream_filter_filter(stream_filter_, stream_context_, NULL,
                                 &converted_data, &converted_size,
                                 packet->data, packet->size,
                                 packet->flags & PKT_FLAG_KEY) < 0) {
    return false;
  }

  // av_bitstream_filter_filter() does not always allocate a new packet.
  // If a new packet was allocated, then we need to modify the
  // |packet| to point to the new data, releasing its current payload
  // if it has the authoritative reference.
  //
  // TODO(ajwong): We're relying on the implementation behavior of
  // av_free_packet() and the meaning of the |destruct| field in
  // AVPacket. Try to find a cleaner way to do this.
  if (converted_data != packet->data) {
    av_free_packet(packet);
    packet->data = converted_data;
    packet->size = converted_size;
    packet->destruct = av_destruct_packet;
  }

  return true;
}

}  // namespace media
