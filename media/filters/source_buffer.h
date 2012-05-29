// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_H_

#include "media/base/demuxer_stream.h"
#include "media/base/stream_parser.h"
#include "media/filters/source_buffer_stream.h"

namespace media {

// SourceBuffer stores media segments and initialization segments that are
// dynamically appended to a particular source ID, hands them to a StreamParser
// to be parsed, and provides buffers to ChunkDemuxerStreams during playback.
class MEDIA_EXPORT SourceBuffer {
 public:
  typedef std::vector<std::pair<base::TimeDelta, base::TimeDelta> > Ranges;

  SourceBuffer();
  ~SourceBuffer();

  typedef base::Callback<void(bool, base::TimeDelta)> InitCB;
  typedef base::Callback<bool(const AudioDecoderConfig&,
                              const VideoDecoderConfig&)> NewConfigCB;
  typedef base::Callback<bool()> NewBuffersCB;
  typedef base::Callback<bool(scoped_array<uint8>, int)> KeyNeededCB;

  void Init(scoped_ptr<StreamParser> parser,
            const InitCB& init_cb,
            const NewConfigCB& config_cb,
            const NewBuffersCB& audio_cb,
            const NewBuffersCB& video_cb,
            const KeyNeededCB& key_needed_cb);

  // Add buffers to this source.  Incoming data is parsed and stored in
  // SourceBufferStreams, which handle ordering and overlap resolution.
  // Returns true if parsing was successful.
  bool AppendData(const uint8* data, size_t length);

  // Fills |out_buffer| with a new buffer from the SourceBufferStream indicated
  // by |type|.
  // Returns true if |out_buffer| is filled with a valid buffer, false if
  // there is not enough data buffered to fulfill the request or if |type| is
  // not a supported stream type.
  bool Read(DemuxerStream::Type type,
            scoped_refptr<StreamParserBuffer>* out_buffer);

  // Seeks the SourceBufferStreams to a new |time|.
  void Seek(base::TimeDelta time);

  // Returns true if this SourceBuffer has seeked to a time without buffered
  // data and is waiting for more data to be appended.
  bool IsSeekPending() const;

  // Resets StreamParser so it can accept a new segment.
  void ResetParser();

  // Fills |ranges_out| with the Ranges that are currently buffered.
  // Returns false if no data is buffered.
  bool GetBufferedRanges(Ranges* ranges_out) const;

  // Notifies all the streams in this SourceBuffer that EndOfStream has been
  // called so that they can return EOS buffers for reads requested after the
  // last buffer in the stream.
  // Returns false if called when there is a gap between the current position
  // and the end of the buffered data.
  bool EndOfStream();

  const AudioDecoderConfig& GetCurrentAudioDecoderConfig();
  const VideoDecoderConfig& GetCurrentVideoDecoderConfig();

 private:
  // StreamParser callbacks.
  void OnStreamParserInitDone(bool success, base::TimeDelta duration);
  bool OnNewConfigs(const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config);
  bool OnAudioBuffers(const StreamParser::BufferQueue& buffer);
  bool OnVideoBuffers(const StreamParser::BufferQueue& buffer);
  bool OnKeyNeeded(scoped_array<uint8> init_data, int init_data_size);

  scoped_ptr<StreamParser> stream_parser_;

  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB audio_cb_;
  NewBuffersCB video_cb_;
  KeyNeededCB key_needed_cb_;

  scoped_ptr<SourceBufferStream> audio_;
  scoped_ptr<SourceBufferStream> video_;

  DISALLOW_COPY_AND_ASSIGN(SourceBuffer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_H_
