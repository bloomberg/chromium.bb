// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamImpl::MojoDemuxerStreamImpl(media::DemuxerStream* stream)
    : stream_(stream), weak_factory_(this) {
}

MojoDemuxerStreamImpl::~MojoDemuxerStreamImpl() {
}

void MojoDemuxerStreamImpl::Read(const mojo::Callback<
    void(mojo::DemuxerStream::Status, mojo::MediaDecoderBufferPtr)>& callback) {
  stream_->Read(base::Bind(&MojoDemuxerStreamImpl::OnBufferReady,
                           weak_factory_.GetWeakPtr(),
                           callback));
}

void MojoDemuxerStreamImpl::OnBufferReady(
    const BufferReadyCB& callback,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  if (status == media::DemuxerStream::kConfigChanged) {
    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    client()->OnAudioDecoderConfigChanged(
        mojo::AudioDecoderConfig::From(stream_->audio_decoder_config()));
  }

  // TODO(tim): Once using DataPipe, fill via the producer handle and then
  // read more to keep the pipe full.
  callback.Run(static_cast<mojo::DemuxerStream::Status>(status),
               mojo::MediaDecoderBuffer::From(buffer));
}

void MojoDemuxerStreamImpl::OnConnectionEstablished() {
  // This is called when our DemuxerStreamClient has connected itself and is
  // ready to receive messages.  Send an initial config and notify it that
  // we are now ready for business.
  client()->OnAudioDecoderConfigChanged(
      mojo::AudioDecoderConfig::From(stream_->audio_decoder_config()));

  // TODO(tim): Create a DataPipe, hold the producer handle, and pass the
  // consumer handle here.
  client()->OnStreamReady(mojo::ScopedDataPipeConsumerHandle());
}

}  // namespace media
