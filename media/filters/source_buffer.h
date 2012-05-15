// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_SOURCE_BUFFER_H_
#define MEDIA_FILTERS_SOURCE_BUFFER_H_

#include "media/base/audio_decoder_config.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"

namespace media {

// SourceBuffer will store media segments and initialization segments that are
// dynamically appended to a particular source ID, hand them to a StreamParser
// to be parsed, and provide buffers to ChunkDemuxerStreams during playback.
class MEDIA_EXPORT SourceBuffer {
 public:
  SourceBuffer();
  ~SourceBuffer();

  typedef base::Callback<void(bool, base::TimeDelta)> InitCB;
  typedef base::Callback<bool(const AudioDecoderConfig&,
                              const VideoDecoderConfig&)> NewConfigCB;
  typedef base::Callback<bool(const StreamParser::BufferQueue&)> NewBuffersCB;
  typedef base::Callback<bool(scoped_array<uint8>, int)> KeyNeededCB;

  void Init(scoped_ptr<StreamParser> parser,
            const InitCB& init_cb,
            const NewConfigCB& config_cb,
            const NewBuffersCB& audio_cb,
            const NewBuffersCB& video_cb,
            const KeyNeededCB& key_needed_cb);

  // Add buffers to this source.  Incoming data will be parsed and then stored
  // in SourceBufferStreams, which will handle ordering and overlap resolution.
  // Returns true if data was successfully appended, false if the parse failed.
  bool AppendData(const uint8* data, size_t length);

  // Clears the data from this buffer.  Used during a seek to ensure the next
  // buffers provided during a read are the buffers appended after the seek.
  void Flush();

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

  DISALLOW_COPY_AND_ASSIGN(SourceBuffer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_SOURCE_BUFFER_H_
