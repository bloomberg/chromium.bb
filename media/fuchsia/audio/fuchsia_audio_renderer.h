// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_RENDERER_H_
#define MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_RENDERER_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/audio_renderer.h"
#include "media/base/buffering_state.h"
#include "media/base/demuxer_stream.h"
#include "media/base/time_source.h"
#include "media/fuchsia/mojom/fuchsia_audio_consumer_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class DecryptingDemuxerStream;
class MediaLog;

// AudioRenderer implementation that output audio to AudioConsumer interface on
// Fuchsia. Unlike the default AudioRendererImpl it doesn't decode audio and
// sends encoded stream directly to AudioConsumer provided by the platform.
class FuchsiaAudioRenderer : public AudioRenderer, public TimeSource {
 public:
  FuchsiaAudioRenderer(
      MediaLog* media_log,
      mojo::PendingRemote<media::mojom::FuchsiaAudioConsumerProvider>
          audio_consumer_provider);
  ~FuchsiaAudioRenderer() final;

  // AudioRenderer implementation.
  void Initialize(DemuxerStream* stream,
                  CdmContext* cdm_context,
                  RendererClient* client,
                  const PipelineStatusCB& init_cb) final;
  TimeSource* GetTimeSource() final;
  void Flush(base::OnceClosure callback) final;
  void StartPlaying() final;
  void SetVolume(float volume) final;

  // TimeSource implementation.
  void StartTicking() final;
  void StopTicking() final;
  void SetPlaybackRate(double playback_rate) final;
  void SetMediaTime(base::TimeDelta time) final;
  base::TimeDelta CurrentMediaTime() final;
  bool GetWallClockTimes(const std::vector<base::TimeDelta>& media_timestamps,
                         std::vector<base::TimeTicks>* wall_clock_times) final;

 private:
  enum class PlaybackState {
    kStopped,

    // We've called Start(), but haven't received updated state. |start_time_|
    // should not be used yet.
    kStarting,

    kPlaying,

    // We've reached end of stream from the demuxer,
    kEndOfStream,
  };

  // Struct used to store state of an input buffer shared with the
  // |stream_sink_|.
  struct StreamSinkBuffer {
    zx::vmo vmo;
    bool is_used = false;
  };

  // Resets AudioConsumer and reports error to the |client_|.
  void OnError(PipelineStatus Status);

  // Callback for DecryptingDemuxerStream::Initialize().
  void OnDecryptorInitialized(PipelineStatus status);

  // Helpers to receive AudioConsumerStatus from the |audio_consumer_|.
  void RequestAudioConsumerStatus();
  void OnAudioConsumerStatusChanged(fuchsia::media::AudioConsumerStatus status);

  // Helpers to pump data from |demuxer_stream_| to |stream_sink_|.
  void ScheduleReadDemuxerStream();
  void ReadDemuxerStream();
  void OnDemuxerStreamReadDone(DemuxerStream::Status status,
                               scoped_refptr<DecoderBuffer> buffer);

  // Result handler for StreamSink::SendPacket().
  void OnStreamSendDone(size_t buffer_index);

  // Updates buffer state and notifies the |client_| if necessary.
  void SetBufferState(BufferingState buffer_state);

  // Discards all pending packets.
  void FlushInternal();

  // End-of-stream event handler for |audio_consumer_|.
  void OnEndOfStream();

  // Calculates media position based on the TimelineFunction returned from
  // AudioConsumer.
  base::TimeDelta CurrentMediaTimeLocked()
      EXCLUSIVE_LOCKS_REQUIRED(state_lock_);

  MediaLog* const media_log_;

  // Handle for |audio_consumer_|. It's stored here until Initialize() is
  // called.
  fidl::InterfaceHandle<fuchsia::media::AudioConsumer> audio_consumer_handle_;

  fuchsia::media::AudioConsumerPtr audio_consumer_;
  fuchsia::media::StreamSinkPtr stream_sink_;
  fuchsia::media::audio::VolumeControlPtr volume_control_;

  DemuxerStream* demuxer_stream_ = nullptr;
  RendererClient* client_ = nullptr;

  // Initialize() completion callback.
  PipelineStatusCB init_cb_;

  std::unique_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  BufferingState buffer_state_ = BUFFERING_HAVE_NOTHING;

  base::TimeDelta last_packet_timestamp_ = base::TimeDelta::Min();
  base::OneShotTimer read_timer_;

  std::vector<StreamSinkBuffer> stream_sink_buffers_;
  size_t num_pending_packets_ = 0u;

  // Lead time range requested by the |audio_consumer_|. Initialized to 0 until
  // the initial AudioConsumerStatus is received.
  base::TimeDelta min_lead_time_;
  base::TimeDelta max_lead_time_;

  // TimeSource interface is not single-threaded. It's used to guard fields
  // that area accessed in the TimeSource implementation.
  base::Lock state_lock_;

  PlaybackState state_ GUARDED_BY(state_lock_) = PlaybackState::kStopped;

  // Values from TimelineFunction returned by AudioConsumer.
  base::TimeTicks reference_time_ GUARDED_BY(state_lock_);
  base::TimeDelta media_pos_ GUARDED_BY(state_lock_);
  int32_t media_delta_ GUARDED_BY(state_lock_) = 1;
  int32_t reference_delta_ GUARDED_BY(state_lock_) = 1;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<FuchsiaAudioRenderer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_AUDIO_FUCHSIA_AUDIO_RENDERER_H_