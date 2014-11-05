// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_adapter.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/services/media_type_converters.h"

namespace media {

MojoDemuxerStreamAdapter::MojoDemuxerStreamAdapter(
    mojo::DemuxerStreamPtr demuxer_stream,
    const base::Closure& stream_ready_cb)
    : demuxer_stream_(demuxer_stream.Pass()),
      stream_ready_cb_(stream_ready_cb),
      type_(DemuxerStream::UNKNOWN),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  demuxer_stream_.set_client(this);
}

MojoDemuxerStreamAdapter::~MojoDemuxerStreamAdapter() {
  DVLOG(1) << __FUNCTION__;
}

void MojoDemuxerStreamAdapter::Read(const DemuxerStream::ReadCB& read_cb) {
  DVLOG(3) << __FUNCTION__;
  // We shouldn't be holding on to a previous callback if a new Read() came in.
  DCHECK(read_cb_.is_null());
  read_cb_ = read_cb;
  demuxer_stream_->Read(base::Bind(&MojoDemuxerStreamAdapter::OnBufferReady,
                                   weak_factory_.GetWeakPtr()));
}

AudioDecoderConfig MojoDemuxerStreamAdapter::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  DCHECK(!audio_config_queue_.empty());
  return audio_config_queue_.front();
}

VideoDecoderConfig MojoDemuxerStreamAdapter::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  DCHECK(!video_config_queue_.empty());
  return video_config_queue_.front();
}

DemuxerStream::Type MojoDemuxerStreamAdapter::type() {
  return type_;
}

void MojoDemuxerStreamAdapter::EnableBitstreamConverter() {
  NOTIMPLEMENTED();
}

bool MojoDemuxerStreamAdapter::SupportsConfigChanges() {
  return true;
}

VideoRotation MojoDemuxerStreamAdapter::video_rotation() {
  NOTIMPLEMENTED();
  return VIDEO_ROTATION_0;
}

void MojoDemuxerStreamAdapter::OnStreamReady(
    mojo::ScopedDataPipeConsumerHandle pipe) {
  DVLOG(1) << __FUNCTION__;
  // TODO(tim): We don't support pipe streaming yet.
  DCHECK(!pipe.is_valid());
  DCHECK_NE(type_, DemuxerStream::UNKNOWN);
  stream_ready_cb_.Run();
}

void MojoDemuxerStreamAdapter::OnAudioDecoderConfigChanged(
    mojo::AudioDecoderConfigPtr config) {
  DCHECK(type_ == DemuxerStream::UNKNOWN || type_ == DemuxerStream::AUDIO)
      << type_;
  type_ = DemuxerStream::AUDIO;

  audio_config_queue_.push(config.To<AudioDecoderConfig>());

  if (!read_cb_.is_null()) {
    read_cb_.Run(DemuxerStream::Status::kConfigChanged, NULL);
    read_cb_.Reset();
  }
}

void MojoDemuxerStreamAdapter::OnVideoDecoderConfigChanged(
    mojo::VideoDecoderConfigPtr config) {
  DCHECK(type_ == DemuxerStream::UNKNOWN || type_ == DemuxerStream::VIDEO)
      << type_;
  type_ = DemuxerStream::VIDEO;

  video_config_queue_.push(config.To<VideoDecoderConfig>());

  if (!read_cb_.is_null()) {
    read_cb_.Run(DemuxerStream::Status::kConfigChanged, NULL);
    read_cb_.Reset();
  }
}

void MojoDemuxerStreamAdapter::OnBufferReady(
    mojo::DemuxerStream::Status status,
    mojo::MediaDecoderBufferPtr buffer) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(!read_cb_.is_null());
  DCHECK_NE(type_, DemuxerStream::UNKNOWN);

  DemuxerStream::Status media_status(
      static_cast<DemuxerStream::Status>(status));
  scoped_refptr<DecoderBuffer> media_buffer(
      buffer.To<scoped_refptr<DecoderBuffer>>());

  if (status == mojo::DemuxerStream::STATUS_CONFIG_CHANGED) {
    DCHECK(!media_buffer.get());

    // If the configuration queue is empty we need to wait for a config change
    // event before invoking |read_cb_|.

    if (type_ == DemuxerStream::AUDIO) {
      audio_config_queue_.pop();
      if (audio_config_queue_.empty())
        return;
    } else if (type_ == DemuxerStream::VIDEO) {
      video_config_queue_.pop();
      if (video_config_queue_.empty())
        return;
    }
  }

  read_cb_.Run(media_status, media_buffer);
  read_cb_.Reset();
}

}  // namespace media
