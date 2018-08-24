// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/audio_decoder/assistant_audio_decoder.h"

#include "base/bind_helpers.h"
#include "media/base/audio_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/formats/mpeg/mpeg1_audio_stream_parser.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "services/service_manager/public/cpp/service_context_ref.h"

namespace chromeos {
namespace assistant {

AssistantAudioDecoder::AssistantAudioDecoder(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref,
    mojom::AssistantAudioDecoderClientPtr client)
    : service_ref_(std::move(service_ref)),
      client_(std::move(client)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_log_(std::make_unique<media::MediaLog>()),
      parser_(std::make_unique<media::MPEG1AudioStreamParser>()),
      decoder_(std::make_unique<media::FFmpegAudioDecoder>(task_runner_,
                                                           media_log_.get())) {
  parser_->Init(
      /*init_cb=*/base::DoNothing(),
      base::BindRepeating(&AssistantAudioDecoder::OnNewConfigs,
                          base::Unretained(this)),
      base::BindRepeating(&AssistantAudioDecoder::OnNewBuffers,
                          base::Unretained(this)),
      /*ignore_text_tracks=*/true,
      /*encrypted_media_init_data_cb=*/base::DoNothing(),
      /*new_segment_cb=*/base::DoNothing(),
      /*end_of_segment_cb=*/base::DoNothing(), media_log_.get());
}

AssistantAudioDecoder::~AssistantAudioDecoder() = default;

void AssistantAudioDecoder::Decode(const std::vector<uint8_t>& data) {
  if (!parser_->Parse(data.data(), data.size()))
    client_->OnDecoderError();
}

bool AssistantAudioDecoder::OnNewConfigs(
    std::unique_ptr<media::MediaTracks> tracks,
    const media::StreamParser::TextTrackConfigMap& text_configs) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  const media::StreamParser::TrackId track_id =
      tracks->tracks().back()->bytestream_track_id();

  // Initialize decoder.
  const media::AudioDecoderConfig& config = tracks->getAudioConfig(track_id);
  decoder_->Initialize(
      config, /*cdm_context=*/nullptr,
      base::BindRepeating(&AssistantAudioDecoder::OnDecoderInitialized,
                          base::Unretained(this), config),
      base::BindRepeating(&AssistantAudioDecoder::OnDecodeOutput,
                          base::Unretained(this)),
      /*waiting_for_decryption_key_cb=*/base::NullCallback());
  return true;
}

bool AssistantAudioDecoder::OnNewBuffers(
    const media::StreamParser::BufferQueueMap& buffer_queue_map) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  int new_buffer_size = 0;
  for (const auto& queue_map : buffer_queue_map) {
    new_buffer_size += queue_map.second.size();
    for (const auto& buffer : queue_map.second)
      parsed_buffers_.push_back(buffer);
  }
  client_->OnNewBuffers(new_buffer_size);
  DecodeNow();
  return true;
}

void AssistantAudioDecoder::OnDecoderInitialized(
    media::AudioDecoderConfig config,
    bool success) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnDecoderInitialized(success, config.samples_per_second(),
                                config.channels());
}

void AssistantAudioDecoder::OnDecodeOutput(
    const scoped_refptr<media::AudioBuffer>& decoded) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  client_->OnBufferDecoded(media::mojom::AudioBuffer::From(decoded));
  DecodeNow();
}

void AssistantAudioDecoder::OnDecodeStatus(media::DecodeStatus status) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (status != media::DecodeStatus::OK)
    client_->OnDecoderError();
}

void AssistantAudioDecoder::DecodeNow() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!parsed_buffers_.empty()) {
    const auto& data = parsed_buffers_.front();
    DCHECK_NE(data->timestamp(), media::kNoTimestamp);

    decoder_->Decode(data,
                     base::BindRepeating(&AssistantAudioDecoder::OnDecodeStatus,
                                         base::Unretained(this)));
    parsed_buffers_.pop_front();
  }
}

}  // namespace assistant
}  // namespace chromeos
