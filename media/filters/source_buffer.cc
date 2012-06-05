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

bool SourceBuffer::Read(DemuxerStream::Type type,
                        scoped_refptr<StreamParserBuffer>* out_buffer) {
  if (type == DemuxerStream::AUDIO) {
    return audio_->GetNextBuffer(out_buffer);
  } else if (type == DemuxerStream::VIDEO) {
    return video_->GetNextBuffer(out_buffer);
  }
  return false;
}

void SourceBuffer::Seek(base::TimeDelta time) {
  if (audio_.get())
    audio_->Seek(time);

  if (video_.get())
    video_->Seek(time);
}

bool SourceBuffer::IsSeekPending() const {
  bool seek_pending = false;

  if (audio_.get())
    seek_pending = audio_->IsSeekPending();

  if (!seek_pending && video_.get())
    seek_pending = video_->IsSeekPending();

  return seek_pending;
}

void SourceBuffer::ResetParser() {
  stream_parser_->Flush();
}

bool SourceBuffer::GetBufferedRanges(Ranges* ranges_out) const {
  // TODO(annacc): calculate buffered ranges. (crbug.com/129852)
  return false;
}

bool SourceBuffer::EndOfStream() {

  if ((audio_.get() && !audio_->CanEndOfStream()) ||
      (video_.get() && !video_->CanEndOfStream()))
    return false;

  if (audio_.get())
    audio_->EndOfStream();

  if (video_.get())
    video_->EndOfStream();

  // Run callbacks so that any pending read requests can be fulfilled.
  if (!audio_cb_.is_null())
    audio_cb_.Run();

  if (!video_cb_.is_null())
    video_cb_.Run();

  return true;
}

const AudioDecoderConfig& SourceBuffer::GetCurrentAudioDecoderConfig() {
  return audio_->GetCurrentAudioDecoderConfig();
}

const VideoDecoderConfig& SourceBuffer::GetCurrentVideoDecoderConfig() {
  return video_->GetCurrentVideoDecoderConfig();
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
  if (audio_config.IsValidConfig()) {
    // Disallow multiple audio configs and new audio configs after stream is
    // initialized.
    if (audio_cb_.is_null() || audio_.get())
      return false;

    audio_.reset(new SourceBufferStream(audio_config));
  }

  if (video_config.IsValidConfig()) {
    // Disallow multiple video configs and new video configs after stream is
    // initialized.
    if (video_cb_.is_null() || video_.get())
      return false;

    video_.reset(new SourceBufferStream(video_config));
  }

  return config_cb_.Run(audio_config, video_config);
}

bool SourceBuffer::OnAudioBuffers(const StreamParser::BufferQueue& buffer) {
  if (!audio_.get())
    return false;
  audio_->Append(buffer);

  return audio_cb_.Run();
}

bool SourceBuffer::OnVideoBuffers(const StreamParser::BufferQueue& buffer) {
  if (!video_.get())
    return false;
  video_->Append(buffer);

  return video_cb_.Run();
}

bool SourceBuffer::OnKeyNeeded(scoped_array<uint8> init_data,
                               int init_data_size) {
  return key_needed_cb_.Run(init_data.Pass(), init_data_size);
}

}  // namespace media
