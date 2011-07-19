// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_H264_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_H264_BITSTREAM_CONVERTER_H_

#include "base/basictypes.h"
#include "media/base/h264_bitstream_converter.h"
#include "media/filters/bitstream_converter.h"

// Forward declarations for FFmpeg datatypes used.
struct AVCodecContext;
struct AVPacket;

namespace media {

// FFmpegH264BitstreamConverter does the conversion on single NAL unit
// basis which is contained within the AVPacket given to ConvertPacket() member
// function with the exception of the first packet which is prepended with the
// AVC decoder configuration record information. For example:
//
//    NAL unit #1  ==> bytestream buffer #1 (AVC configuraion + NAL unit #1)
//    NAL unit #2  ==> bytestream buffer #2 (NAL unit #2)
//    ...
//    NAL unit #n  ==> bytestream buffer #n (NAL unit #n)
//
// User of the object can append output into one bigger buffer by the client if
// efficiency reasons warrants the client to do so.
//
// FFmpegH264BitstreamConverter acts as an adapter for the H264 Bitstream
// converter class by implementing the BitstreamConverter interface expected
// by media pipeline streams demuxed by FFmpegDemuxer.
//
// FFmpegH264BitstreamConverter uses FFmpeg allocation methods for buffer
// allocation to ensure compatibility with FFmpeg's memory management.

class FFmpegH264BitstreamConverter : public BitstreamConverter {
 public:
  // The |stream_context| will be used during conversion and should be the
  // AVCodecContext for the stream sourcing these packets. A reference to
  // |stream_context| is retained, so it must outlive this class.
  explicit FFmpegH264BitstreamConverter(AVCodecContext* stream_context);
  virtual ~FFmpegH264BitstreamConverter();

  // BitstreamConverter implementation
  virtual bool Initialize();
  virtual bool ConvertPacket(AVPacket* packet);

 private:
  // Actual converter class.
  H264BitstreamConverter converter_;

  // Flag for indicating whether global parameter sets have been processed.
  bool configuration_processed_;

  // Variable to hold a pointer to memory where we can access the global
  // data from the FFmpeg file format's global headers.
  AVCodecContext* stream_context_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegH264BitstreamConverter);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_H264_BITSTREAM_CONVERTER_H_

