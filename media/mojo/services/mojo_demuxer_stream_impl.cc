// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/services/media_type_converters.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamImpl::MojoDemuxerStreamImpl(
    media::DemuxerStream* stream,
    mojo::InterfaceRequest<interfaces::DemuxerStream> request)
    : binding_(this, std::move(request)),
      stream_(stream),
      weak_factory_(this) {}

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
  stream_pipe_ = std::move(data_pipe.producer_handle);

  // Prepare the initial config.
  interfaces::AudioDecoderConfigPtr audio_config;
  interfaces::VideoDecoderConfigPtr video_config;
  if (stream_->type() == media::DemuxerStream::AUDIO) {
    audio_config =
        interfaces::AudioDecoderConfig::From(stream_->audio_decoder_config());
  } else if (stream_->type() == media::DemuxerStream::VIDEO) {
    video_config =
        interfaces::VideoDecoderConfig::From(stream_->video_decoder_config());
  } else {
    NOTREACHED() << "Unsupported stream type: " << stream_->type();
    return;
  }

  callback.Run(static_cast<interfaces::DemuxerStream::Type>(stream_->type()),
               std::move(data_pipe.consumer_handle), std::move(audio_config),
               std::move(video_config));
}

void MojoDemuxerStreamImpl::Read(const ReadCallback& callback)  {
  stream_->Read(base::Bind(&MojoDemuxerStreamImpl::OnBufferReady,
                           weak_factory_.GetWeakPtr(), callback));
}

void MojoDemuxerStreamImpl::OnBufferReady(
    const ReadCallback& callback,
    media::DemuxerStream::Status status,
    const scoped_refptr<media::DecoderBuffer>& buffer) {
  interfaces::AudioDecoderConfigPtr audio_config;
  interfaces::VideoDecoderConfigPtr video_config;

  if (status == media::DemuxerStream::kConfigChanged) {
    DVLOG(2) << __FUNCTION__ << ": ConfigChange!";
    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    if (stream_->type() == media::DemuxerStream::AUDIO) {
      audio_config =
          interfaces::AudioDecoderConfig::From(stream_->audio_decoder_config());
    } else if (stream_->type() == media::DemuxerStream::VIDEO) {
      video_config =
          interfaces::VideoDecoderConfig::From(stream_->video_decoder_config());
    } else {
      NOTREACHED() << "Unsupported config change encountered for type: "
                   << stream_->type();
    }

    callback.Run(interfaces::DemuxerStream::STATUS_CONFIG_CHANGED,
                 interfaces::DecoderBufferPtr(), std::move(audio_config),
                 std::move(video_config));
    return;
  }

  if (status == media::DemuxerStream::kAborted) {
    callback.Run(interfaces::DemuxerStream::STATUS_ABORTED,
                 interfaces::DecoderBufferPtr(), std::move(audio_config),
                 std::move(video_config));
    return;
  }

  DCHECK_EQ(status, media::DemuxerStream::kOk);
  if (!buffer->end_of_stream()) {
    DCHECK_GT(buffer->data_size(), 0u);
    // Serialize the data section of the DecoderBuffer into our pipe.
    uint32_t bytes_to_write = base::checked_cast<uint32_t>(buffer->data_size());
    uint32_t bytes_written = bytes_to_write;
    CHECK_EQ(WriteDataRaw(stream_pipe_.get(), buffer->data(), &bytes_written,
                          MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(bytes_to_write, bytes_written);
  }

  // TODO(dalecurtis): Once we can write framed data to the DataPipe, fill via
  // the producer handle and then read more to keep the pipe full.  Waiting for
  // space can be accomplished using an AsyncWaiter.
  callback.Run(static_cast<interfaces::DemuxerStream::Status>(status),
               interfaces::DecoderBuffer::From(buffer), std::move(audio_config),
               std::move(video_config));
}

}  // namespace media
