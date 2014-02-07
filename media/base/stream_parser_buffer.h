// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_STREAM_PARSER_BUFFER_H_
#define MEDIA_BASE_STREAM_PARSER_BUFFER_H_

#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class MEDIA_EXPORT StreamParserBuffer : public DecoderBuffer {
 public:
  // Value used to signal an invalid decoder config ID.
  enum { kInvalidConfigId = -1 };

  typedef DemuxerStream::Type Type;
  typedef StreamParser::TrackId TrackId;

  static scoped_refptr<StreamParserBuffer> CreateEOSBuffer();
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size, bool is_keyframe, Type type,
      TrackId track_id);
  static scoped_refptr<StreamParserBuffer> CopyFrom(
      const uint8* data, int data_size,
      const uint8* side_data, int side_data_size, bool is_keyframe, Type type,
      TrackId track_id);
  bool IsKeyframe() const { return is_keyframe_; }

  // Decode timestamp. If not explicitly set, or set to kNoTimestamp(), the
  // value will be taken from the normal timestamp.
  base::TimeDelta GetDecodeTimestamp() const;
  void SetDecodeTimestamp(const base::TimeDelta& timestamp);

  // Gets/sets the ID of the decoder config associated with this buffer.
  int GetConfigId() const;
  void SetConfigId(int config_id);

  // Gets the parser's media type associated with this buffer. Value is
  // meaningless for EOS buffers.
  Type type() const { return type_; }

  // Gets the parser's track ID associated with this buffer. Value is
  // meaningless for EOS buffers.
  TrackId track_id() const { return track_id_; }

  // Buffers to be exhausted before using the data in this DecoderBuffer.  Used
  // to implement the Audio Splice Frame Algorithm per the MSE specification.
  const std::vector<scoped_refptr<StreamParserBuffer> >& GetFadeOutPreroll()
      const;
  void SetFadeOutPreroll(
      const std::vector<scoped_refptr<StreamParserBuffer> >& fade_out_preroll);

 private:
  StreamParserBuffer(const uint8* data, int data_size,
                     const uint8* side_data, int side_data_size,
                     bool is_keyframe, Type type,
                     TrackId track_id);
  virtual ~StreamParserBuffer();

  bool is_keyframe_;
  base::TimeDelta decode_timestamp_;
  int config_id_;
  Type type_;
  TrackId track_id_;

  std::vector<scoped_refptr<StreamParserBuffer> > fade_out_preroll_;

  DISALLOW_COPY_AND_ASSIGN(StreamParserBuffer);
};

}  // namespace media

#endif  // MEDIA_BASE_STREAM_PARSER_BUFFER_H_
