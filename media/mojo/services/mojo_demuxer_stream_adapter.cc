// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_adapter.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/services/media_type_converters.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamAdapter::MojoDemuxerStreamAdapter(
    interfaces::DemuxerStreamPtr demuxer_stream,
    const base::Closure& stream_ready_cb)
    : demuxer_stream_(std::move(demuxer_stream)),
      stream_ready_cb_(stream_ready_cb),
      type_(DemuxerStream::UNKNOWN),
      weak_factory_(this) {
  DVLOG(1) << __FUNCTION__;
  demuxer_stream_->Initialize(base::Bind(
      &MojoDemuxerStreamAdapter::OnStreamReady, weak_factory_.GetWeakPtr()));
}

MojoDemuxerStreamAdapter::~MojoDemuxerStreamAdapter() {
  DVLOG(1) << __FUNCTION__;
}

void MojoDemuxerStreamAdapter::Read(const DemuxerStream::ReadCB& read_cb) {
  DVLOG(3) << __FUNCTION__;
  // We shouldn't be holding on to a previous callback if a new Read() came in.
  DCHECK(read_cb_.is_null());

  DCHECK(stream_pipe_.is_valid());
  read_cb_ = read_cb;
  demuxer_stream_->Read(base::Bind(&MojoDemuxerStreamAdapter::OnBufferReady,
                                   weak_factory_.GetWeakPtr()));
}

AudioDecoderConfig MojoDemuxerStreamAdapter::audio_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::AUDIO);
  return audio_config_;
}

VideoDecoderConfig MojoDemuxerStreamAdapter::video_decoder_config() {
  DCHECK_EQ(type_, DemuxerStream::VIDEO);
  return video_config_;
}

DemuxerStream::Type MojoDemuxerStreamAdapter::type() const {
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

// TODO(xhwang): Pass liveness here.
void MojoDemuxerStreamAdapter::OnStreamReady(
    interfaces::DemuxerStream::Type type,
    mojo::ScopedDataPipeConsumerHandle pipe,
    interfaces::AudioDecoderConfigPtr audio_config,
    interfaces::VideoDecoderConfigPtr video_config) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(pipe.is_valid());
  DCHECK_EQ(DemuxerStream::UNKNOWN, type_);

  type_ = static_cast<DemuxerStream::Type>(type);
  stream_pipe_ = std::move(pipe);
  UpdateConfig(std::move(audio_config), std::move(video_config));

  stream_ready_cb_.Run();
}

void MojoDemuxerStreamAdapter::OnBufferReady(
    interfaces::DemuxerStream::Status status,
    interfaces::DecoderBufferPtr buffer,
    interfaces::AudioDecoderConfigPtr audio_config,
    interfaces::VideoDecoderConfigPtr video_config) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(!read_cb_.is_null());
  DCHECK_NE(type_, DemuxerStream::UNKNOWN);
  DCHECK(stream_pipe_.is_valid());

  if (status == interfaces::DemuxerStream::STATUS_CONFIG_CHANGED) {
    UpdateConfig(std::move(audio_config), std::move(video_config));
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kConfigChanged, nullptr);
    return;
  }

  if (status == interfaces::DemuxerStream::STATUS_ABORTED) {
    base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kAborted, nullptr);
    return;
  }

  DCHECK_EQ(status, interfaces::DemuxerStream::STATUS_OK);
  scoped_refptr<DecoderBuffer> media_buffer(
      buffer.To<scoped_refptr<DecoderBuffer>>());

  if (!media_buffer->end_of_stream()) {
    DCHECK_GT(media_buffer->data_size(), 0u);

    // Wait for the data to become available in the DataPipe.
    MojoHandleSignalsState state;
    CHECK_EQ(MOJO_RESULT_OK,
             MojoWait(stream_pipe_.get().value(), MOJO_HANDLE_SIGNAL_READABLE,
                      MOJO_DEADLINE_INDEFINITE, &state));
    CHECK_EQ(MOJO_HANDLE_SIGNAL_READABLE, state.satisfied_signals);

    // Read the inner data for the DecoderBuffer from our DataPipe.
    uint32_t bytes_to_read =
        base::checked_cast<uint32_t>(media_buffer->data_size());
    uint32_t bytes_read = bytes_to_read;
    CHECK_EQ(ReadDataRaw(stream_pipe_.get(), media_buffer->writable_data(),
                         &bytes_read, MOJO_READ_DATA_FLAG_ALL_OR_NONE),
             MOJO_RESULT_OK);
    CHECK_EQ(bytes_to_read, bytes_read);
  }

  base::ResetAndReturn(&read_cb_).Run(DemuxerStream::kOk, media_buffer);
}

void MojoDemuxerStreamAdapter::UpdateConfig(
    interfaces::AudioDecoderConfigPtr audio_config,
    interfaces::VideoDecoderConfigPtr video_config) {
  DCHECK_NE(type_, DemuxerStream::UNKNOWN);

  switch(type_) {
    case DemuxerStream::AUDIO:
      DCHECK(audio_config && !video_config);
      audio_config_ = audio_config.To<AudioDecoderConfig>();
      break;
    case DemuxerStream::VIDEO:
      DCHECK(video_config && !audio_config);
      video_config_ = video_config.To<VideoDecoderConfig>();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace media
