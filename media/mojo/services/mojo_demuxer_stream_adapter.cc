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
      weak_factory_(this) {
  demuxer_stream_.set_client(this);
}

MojoDemuxerStreamAdapter::~MojoDemuxerStreamAdapter() {
}

void MojoDemuxerStreamAdapter::Read(const DemuxerStream::ReadCB& read_cb) {
  // We shouldn't be holding on to a previous callback if a new Read() came in.
  DCHECK(read_cb_.is_null());
  read_cb_ = read_cb;
  demuxer_stream_->Read(base::Bind(&MojoDemuxerStreamAdapter::OnBufferReady,
                                   weak_factory_.GetWeakPtr()));
}

AudioDecoderConfig MojoDemuxerStreamAdapter::audio_decoder_config() {
  DCHECK(!config_queue_.empty());
  return config_queue_.front();
}

VideoDecoderConfig MojoDemuxerStreamAdapter::video_decoder_config() {
  NOTREACHED();
  return VideoDecoderConfig();
}

media::DemuxerStream::Type MojoDemuxerStreamAdapter::type() {
  return media::DemuxerStream::AUDIO;
}

void MojoDemuxerStreamAdapter::EnableBitstreamConverter() {
  NOTREACHED();
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
  // TODO(tim): We don't support pipe streaming yet.
  DCHECK(!pipe.is_valid());
  DCHECK(!config_queue_.empty());
  stream_ready_cb_.Run();
}

void MojoDemuxerStreamAdapter::OnAudioDecoderConfigChanged(
    mojo::AudioDecoderConfigPtr config) {
  config_queue_.push(config.To<media::AudioDecoderConfig>());

  if (!read_cb_.is_null()) {
    read_cb_.Run(media::DemuxerStream::Status::kConfigChanged, NULL);
    read_cb_.Reset();
  }
}

void MojoDemuxerStreamAdapter::OnBufferReady(
    mojo::DemuxerStream::Status status,
    mojo::MediaDecoderBufferPtr buffer) {
  DCHECK(!read_cb_.is_null());
  DCHECK(!config_queue_.empty());

  media::DemuxerStream::Status media_status(
      static_cast<media::DemuxerStream::Status>(status));
  scoped_refptr<media::DecoderBuffer> media_buffer(
      buffer.To<scoped_refptr<media::DecoderBuffer> >());

  if (status == mojo::DemuxerStream::STATUS_CONFIG_CHANGED) {
    DCHECK(!media_buffer.get());
    config_queue_.pop();

    // If the |config_queue_| is empty we need to wait for
    // OnAudioDecoderConfigChanged before invoking |read_cb|.
    if (config_queue_.empty())
      return;
  }
  read_cb_.Run(media_status, media_buffer);
  read_cb_.Reset();
}

}  // namespace media
