// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP2T_ES_PARSER_H264_H_
#define MEDIA_MP2T_ES_PARSER_H264_H_

#include <list>
#include <utility>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "media/base/byte_queue.h"
#include "media/base/video_decoder_config.h"
#include "media/mp2t/es_parser.h"

namespace media {
class BitReader;
class StreamParserBuffer;
}

namespace media {
namespace mp2t {

// Remark:
// In this h264 parser, frame splitting is based on AUD nals.
// Mpeg2 TS spec: "2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video"
// "Each AVC access unit shall contain an access unit delimiter NAL Unit;"
//
class EsParserH264 : public EsParser {
 public:
  typedef base::Callback<void(const VideoDecoderConfig&)> NewVideoConfigCB;

  EsParserH264(const NewVideoConfigCB& new_video_config_cb,
               const EmitBufferCB& emit_buffer_cb);
  virtual ~EsParserH264();

  // EsParser implementation.
  virtual bool Parse(const uint8* buf, int size,
                     base::TimeDelta pts,
                     base::TimeDelta dts) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual void Reset() OVERRIDE;

 private:
  struct TimingDesc {
    base::TimeDelta dts;
    base::TimeDelta pts;
  };

  // H264 parser.
  // It resumes parsing from byte position |es_pos_|.
  bool ParseInternal();

  // Emit a frame if a frame has been started earlier.
  void EmitFrameIfNeeded(int next_aud_pos);

  // Start a new frame.
  // Note: if aud_pos < 0, clear the current frame.
  void StartFrame(int aud_pos);

  // Discard |nbytes| of ES from the ES byte queue.
  void DiscardEs(int nbytes);

  // Parse a NAL / SPS.
  // Returns true if successful (compliant bitstream).
  bool NalParser(const uint8* buf, int size);
  bool ProcessSPS(const uint8* buf, int size);

  // Callbacks to pass the stream configuration and the frames.
  NewVideoConfigCB new_video_config_cb_;
  EmitBufferCB emit_buffer_cb_;

  // Bytes of the ES stream that have not been emitted yet.
  ByteQueue es_byte_queue_;
  std::list<std::pair<int, TimingDesc> > timing_desc_list_;

  // H264 parser state.
  // Note: |current_access_unit_pos_| is pointing to an annexB syncword
  // while |current_nal_pos_| is pointing to the NAL unit
  // (i.e. does not include the annexB syncword).
  int es_pos_;
  int current_nal_pos_;
  int current_access_unit_pos_;
  bool is_key_frame_;

  // Last video decoder config.
  VideoDecoderConfig last_video_decoder_config_;
};

}  // namespace mp2t
}  // namespace media

#endif

