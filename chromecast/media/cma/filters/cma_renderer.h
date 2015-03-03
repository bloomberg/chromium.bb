// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_FILTERS_CMA_RENDERER_H_
#define CHROMECAST_MEDIA_CMA_FILTERS_CMA_RENDERER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "media/base/buffering_state.h"
#include "media/base/renderer.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class DemuxerStreamProvider;
class TimeDeltaInterpolator;
class VideoFrame;
}

namespace chromecast {
namespace media {
class AudioPipeline;
class BalancedMediaTaskRunnerFactory;
class MediaPipeline;
class VideoPipeline;

class CmaRenderer : public ::media::Renderer {
 public:
  explicit CmaRenderer(scoped_ptr<MediaPipeline> media_pipeline);
  ~CmaRenderer() override;

  // ::media::Renderer implementation:
  void Initialize(
      ::media::DemuxerStreamProvider* demuxer_stream_provider,
      const ::media::PipelineStatusCB& init_cb,
      const ::media::StatisticsCB& statistics_cb,
      const ::media::BufferingStateCB& buffering_state_cb,
      const PaintCB& paint_cb,
      const base::Closure& ended_cb,
      const ::media::PipelineStatusCB& error_cb,
      const base::Closure& waiting_for_decryption_key_cb) override;
  void Flush(const base::Closure& flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(float playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  bool HasAudio() override;
  bool HasVideo() override;
  void SetCdm(::media::CdmContext* cdm_context,
              const ::media::CdmAttachedCB& cdm_attached_cb) override;

 private:
  enum State {
    kUninitialized,
    kPlaying,
    kFlushed,
    kError,
  };

  // Asynchronous initialization sequence. These four methods are invoked in
  // the order below, with a successful initialization making it to
  // OnVideoPipelineInitializeDone, regardless of which streams are present.
  void InitializeAudioPipeline();
  void OnAudioPipelineInitializeDone(::media::PipelineStatus status);
  void InitializeVideoPipeline();
  void OnVideoPipelineInitializeDone(::media::PipelineStatus status);

  // Callbacks for AvPipelineClient.
  void OnEosReached(bool is_audio);
  void OnStatisticsUpdated(const ::media::PipelineStatistics& stats);
  void OnNaturalSizeChanged(const gfx::Size& size);

  // Callbacks for MediaPipelineClient.
  void OnPlaybackTimeUpdated(base::TimeDelta time,
                             base::TimeDelta max_time,
                             base::TimeTicks capture_time);
  void OnBufferingNotification(::media::BufferingState state);

  void OnFlushDone(::media::PipelineStatus status);
  void OnError(::media::PipelineStatus status);

  // Begin a state transition.
  // Return true if delayed because of a pending state transition.
  void BeginStateTransition();
  void CompleteStateTransition(State new_state);

  base::ThreadChecker thread_checker_;

  scoped_refptr<BalancedMediaTaskRunnerFactory> media_task_runner_factory_;
  scoped_ptr<MediaPipeline> media_pipeline_;
  AudioPipeline* audio_pipeline_;
  VideoPipeline* video_pipeline_;

  ::media::DemuxerStreamProvider* demuxer_stream_provider_;

  // Set of callbacks.
  PaintCB paint_cb_;
  ::media::PipelineStatusCB init_cb_;
  ::media::StatisticsCB statistics_cb_;
  base::Closure ended_cb_;
  ::media::PipelineStatusCB error_cb_;
  ::media::BufferingStateCB buffering_state_cb_;
  base::Closure flush_cb_;
  base::Closure waiting_for_decryption_key_cb_;

  // Renderer state.
  // Used mostly for checking that transitions are correct.
  State state_;
  bool is_pending_transition_;

  bool has_audio_;
  bool has_video_;

  bool received_audio_eos_;
  bool received_video_eos_;

  // Data members for helping the creation of the initial video hole frame.
  gfx::Size initial_natural_size_;
  bool initial_video_hole_created_;

  // Lock protecting access to |time_interpolator_|.
  base::Lock time_interpolator_lock_;

  // base::TickClock used by |time_interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  scoped_ptr< ::media::TimeDeltaInterpolator> time_interpolator_;

  float playback_rate_;

  base::WeakPtr<CmaRenderer> weak_this_;
  base::WeakPtrFactory<CmaRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CmaRenderer);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_FILTERS_CMA_RENDERER_H_
