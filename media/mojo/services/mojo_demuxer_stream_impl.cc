// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "media/mojo/services/media_type_converters.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamImpl::MojoDemuxerStreamImpl(media::DemuxerStream* stream)
    : stream_(stream), weak_factory_(this) {
}

MojoDemuxerStreamImpl::~MojoDemuxerStreamImpl() {
}

// This is called when our DemuxerStreamClient has connected itself and is
// ready to receive messages.  Send an initial config and notify it that
// we are now ready for business.
void MojoDemuxerStreamImpl::Initialize(const InitializeCallback& callback) {
  DVLOG(2) << __FUNCTION__;
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;

  // Allocate DataPipe sizes based on content type to reduce overhead.  If this
  // is still too burdensome we can adjust for sample rate or resolution.
  if (stream_->type() == media::DemuxerStream::VIDEO) {
    // Video can get quite large; at 4K, VP9 delivers packets which are ~1MB in
    // size; so allow for 50% headroom.
    options.capacity_num_bytes = 1.5 * (1024 * 1024);
  } else {
    // Other types don't require a lot of room, so use a smaller pipe.
    options.capacity_num_bytes = 512 * 1024;
  }

  mojo::DataPipe data_pipe(options);
  stream_pipe_ = data_pipe.producer_handle.Pass();

  // Prepare the initial config.
  mojo::AudioDecoderConfigPtr audio_config;
  mojo::VideoDecoderConfigPtr video_config;
  if (stream_->type() == media::DemuxerStream::AUDIO) {
    audio_config =
        mojo::AudioDecoderConfig::From(stream_->audio_decoder_config());
  } else if (stream_->type() == media::DemuxerStream::VIDEO) {
    video_config =
        mojo::VideoDecoderConfig::From(stream_->video_decoder_config());
  } else {
    NOTREACHED() << "Unsupported stream type: " << stream_->type();
    return;
  }

  callback.Run(static_cast<mojo::DemuxerStream::Type>(stream_->type()),
               data_pipe.consumer_handle.Pass(), audio_config.Pass(),
               video_config.Pass());
}

void MojoDemuxerStreamImpl::Read(const ReadCallback& callback)  {
  stream_->Read(base::Bind(&MojoDemuxerStreamImpl::OnBufferReady,
                           weak_factory_.GetWeakPtr(), callback));
}

void MojoDemuxerStreamImpl::OnBufferReady(
    const ReadCallback& callback,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  mojo::AudioDecoderConfigPtr audio_config;
  mojo::VideoDecoderConfigPtr video_config;

  if (status == media::DemuxerStream::kConfigChanged) {
    DVLOG(2) << __FUNCTION__ << ": ConfigChange!";
    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    if (stream_->type() == media::DemuxerStream::AUDIO) {
      audio_config =
          mojo::AudioDecoderConfig::From(stream_->audio_decoder_config());
    } else if (stream_->type() == media::DemuxerStream::VIDEO) {
      video_config =
          mojo::VideoDecoderConfig::From(stream_->video_decoder_config());
    } else {
      NOTREACHED() << "Unsupported config change encountered for type: "
                   << stream_->type();
    }

    callback.Run(mojo::DemuxerStream::STATUS_CONFIG_CHANGED,
                 mojo::MediaDecoderBufferPtr(), audio_config.Pass(),
                 video_config.Pass());
    return;
  }

  if (status == media::DemuxerStream::kAborted) {
    callback.Run(mojo::DemuxerStream::STATUS_ABORTED,
                 mojo::MediaDecoderBufferPtr(), audio_config.Pass(),
                 video_config.Pass());
    return;
  }

  DCHECK_EQ(status, media::DemuxerStream::kOk);
  if (!buffer->end_of_stream()) {
    DCHECK_GT(buffer->data_size(), 0);
    // Serialize the data section of the DecoderBuffer into our pipe.
    uint32_t num_bytes = buffer->data_size();
    CHECK_EQ(WriteDataRaw(stream_pipe_.get(), buffer->data(), &num_bytes,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(num_bytes, static_cast<uint32_t>(buffer->data_size()));
  }

  // TODO(dalecurtis): Once we can write framed data to the DataPipe, fill via
  // the producer handle and then read more to keep the pipe full.  Waiting for
  // space can be accomplished using an AsyncWaiter.
  callback.Run(static_cast<mojo::DemuxerStream::Status>(status),
               mojo::MediaDecoderBuffer::From(buffer), audio_config.Pass(),
               video_config.Pass());
}

}  // namespace media
