// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_
#define CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "media/base/stream_parser.h"

namespace media {
class AudioBuffer;
class MediaLog;
class MediaTracks;
class MPEG1AudioStreamParser;
class FFmpegAudioDecoder;
class StreamParserBuffer;
}  // namespace media

namespace service_manager {
class ServiceContextRef;
}  // namespace service_manager

namespace chromeos {
namespace assistant {

class AssistantAudioDecoder : public mojom::AssistantAudioDecoder {
 public:
  AssistantAudioDecoder(
      std::unique_ptr<service_manager::ServiceContextRef> service_ref,
      mojom::AssistantAudioDecoderClientPtr client);
  ~AssistantAudioDecoder() override;

  // Called by |client_| on main thread.
  void Decode(const std::vector<uint8_t>& data) override;

 private:
  // Called by |parser_| on main thread.
  bool OnNewConfigs(
      std::unique_ptr<media::MediaTracks> tracks,
      const media::StreamParser::TextTrackConfigMap& text_configs);
  bool OnNewBuffers(
      const media::StreamParser::BufferQueueMap& buffer_queue_map);

  // Called by |decoder_| on main thread.
  void OnDecoderInitialized(media::AudioDecoderConfig config, bool success);
  void OnDecodeOutput(const scoped_refptr<media::AudioBuffer>& decoded);
  void OnDecodeStatus(media::DecodeStatus status);

  // Called by |OnNewBuffers()| and |OnDecodeOutput()| on main thread.
  void DecodeNow();

  const std::unique_ptr<service_manager::ServiceContextRef> service_ref_;
  mojom::AssistantAudioDecoderClientPtr client_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<media::MediaLog> media_log_;
  std::unique_ptr<media::MPEG1AudioStreamParser> parser_;
  std::unique_ptr<media::FFmpegAudioDecoder> decoder_;
  base::circular_deque<scoped_refptr<media::StreamParserBuffer>>
      parsed_buffers_;

  DISALLOW_COPY_AND_ASSIGN(AssistantAudioDecoder);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_AUDIO_DECODER_ASSISTANT_AUDIO_DECODER_H_
