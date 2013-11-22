// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MP3_MP3_STREAM_PARSER_H_
#define MEDIA_MP3_MP3_STREAM_PARSER_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class BitReader;

class MEDIA_EXPORT MP3StreamParser : public StreamParser {
 public:
  MP3StreamParser();
  virtual ~MP3StreamParser();

  // StreamParser implementation.
  virtual void Init(const InitCB& init_cb, const NewConfigCB& config_cb,
                    const NewBuffersCB& new_buffers_cb,
                    const NewTextBuffersCB& text_cb,
                    const NeedKeyCB& need_key_cb,
                    const NewMediaSegmentCB& new_segment_cb,
                    const base::Closure& end_of_segment_cb,
                    const LogCB& log_cb) OVERRIDE;
  virtual void Flush() OVERRIDE;
  virtual bool Parse(const uint8* buf, int size) OVERRIDE;

 private:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    PARSE_ERROR
  };

  State state_;

  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  NewMediaSegmentCB new_segment_cb_;
  base::Closure end_of_segment_cb_;
  LogCB log_cb_;

  ByteQueue queue_;

  AudioDecoderConfig config_;
  scoped_ptr<AudioTimestampHelper> timestamp_helper_;
  bool in_media_segment_;

  void ChangeState(State state);

  // Parsing functions for various byte stream elements.
  // |data| & |size| describe the data available for parsing.
  // These functions are expected to consume an entire frame/header.
  // It should only return a value greater than 0 when |data| has
  // enough bytes to successfully parse & consume the entire element.
  //
  // |frame_size| - Required parameter that is set to the size of the frame, in
  // bytes, including the frame header if the function returns a value > 0.
  // |sample_rate| - Optional parameter that is set to the sample rate
  // of the frame if this function returns a value > 0.
  // |channel_layout| - Optional parameter that is set to the channel_layout
  // of the frame if this function returns a value > 0.
  // |sample_count| - Optional parameter that is set to the number of samples
  // in the frame if this function returns a value > 0.
  //
  // |sample_rate|, |channel_layout|, |sample_count| may be NULL if the caller
  // is not interested in receiving these values from the frame header.
  //
  // Returns:
  // > 0 : The number of bytes parsed.
  //   0 : If more data is needed to parse the entire element.
  // < 0 : An error was encountered during parsing.
  int ParseFrameHeader(const uint8* data, int size,
                       int* frame_size,
                       int* sample_rate,
                       ChannelLayout* channel_layout,
                       int* sample_count) const;
  int ParseMP3Frame(const uint8* data, int size, BufferQueue* buffers);
  int ParseIcecastHeader(const uint8* data, int size);
  int ParseID3v1(const uint8* data, int size);
  int ParseID3v2(const uint8* data, int size);

  // Parses an ID3v2 "sync safe" integer.
  // |reader| - A BitReader to read from.
  // |value| - Set to the integer value read, if true is returned.
  //
  // Returns true if the integer was successfully parsed and |value|
  // was set.
  // Returns false if an error was encountered. The state of |value| is
  // undefined when false is returned.
  bool ParseSyncSafeInt(BitReader* reader, int32* value);

  // Scans |data| for the next valid start code.
  // Returns:
  // > 0 : The number of bytes that should be skipped to reach the
  //       next start code..
  //   0 : If a valid start code was not found and more data is needed.
  // < 0 : An error was encountered during parsing.
  int FindNextValidStartCode(const uint8* data, int size) const;

  // Sends the buffers in |buffers| to |new_buffers_cb_| and then clears
  // |buffers|.
  // If |end_of_segment| is set to true, then |end_of_segment_cb_| is called
  // after |new_buffers_cb_| to signal that these buffers represent the end of a
  // media segment.
  // Returns true if the buffers are sent successfully.
  bool SendBuffers(BufferQueue* buffers, bool end_of_segment);

  DISALLOW_COPY_AND_ASSIGN(MP3StreamParser);
};

}  // namespace media

#endif  // MEDIA_MP3_MP3_STREAM_PARSER_H_
