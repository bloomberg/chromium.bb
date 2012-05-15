// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/source_buffer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/webm/webm_stream_parser.h"

namespace media {

SourceBuffer::SourceBuffer() {}

SourceBuffer::~SourceBuffer() {}

void SourceBuffer::Init(scoped_ptr<StreamParser> parser,
                        const InitCB& init_cb,
                        const NewConfigCB& config_cb,
                        const NewBuffersCB& audio_cb,
                        const NewBuffersCB& video_cb,
                        const KeyNeededCB& key_needed_cb) {
  DCHECK(init_cb_.is_null());
  DCHECK(parser.get());
  DCHECK(!init_cb.is_null());
  DCHECK(!config_cb.is_null());
  DCHECK(!audio_cb.is_null() || !video_cb.is_null());
  DCHECK(!key_needed_cb.is_null());

  init_cb_ = init_cb;
  config_cb_ = config_cb;
  audio_cb_ = audio_cb;
  video_cb_ = video_cb;
  key_needed_cb_ = key_needed_cb;

  stream_parser_.reset(parser.release());

  stream_parser_->Init(
      base::Bind(&SourceBuffer::OnStreamParserInitDone, base::Unretained(this)),
      base::Bind(&SourceBuffer::OnNewConfigs, base::Unretained(this)),
      base::Bind(&SourceBuffer::OnAudioBuffers, base::Unretained(this)),
      base::Bind(&SourceBuffer::OnVideoBuffers, base::Unretained(this)),
      base::Bind(&SourceBuffer::OnKeyNeeded, base::Unretained(this)));
}

bool SourceBuffer::AppendData(const uint8* data, size_t length) {
  return stream_parser_->Parse(data, length);
}

void SourceBuffer::Flush() {
  stream_parser_->Flush();
}

void SourceBuffer::OnStreamParserInitDone(bool success,
                                          base::TimeDelta duration) {
  init_cb_.Run(success, duration);
  init_cb_.Reset();
}

bool SourceBuffer::OnNewConfigs(const AudioDecoderConfig& audio_config,
                    const VideoDecoderConfig& video_config) {
  CHECK(audio_config.IsValidConfig() || video_config.IsValidConfig());

  // Signal an error if we get configuration info for stream types
  // we don't have a callback to handle.
  if ((audio_config.IsValidConfig() && audio_cb_.is_null()) ||
      (video_config.IsValidConfig() && video_cb_.is_null())) {
    return false;
  }

  return config_cb_.Run(audio_config, video_config);
}

bool SourceBuffer::OnAudioBuffers(const StreamParser::BufferQueue& buffer) {
  return audio_cb_.Run(buffer);
}

bool SourceBuffer::OnVideoBuffers(const StreamParser::BufferQueue& buffer) {
  return video_cb_.Run(buffer);
}

bool SourceBuffer::OnKeyNeeded(scoped_array<uint8> init_data,
                               int init_data_size) {
  return key_needed_cb_.Run(init_data.Pass(), init_data_size);
}

}  // namespace media
