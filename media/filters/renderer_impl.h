// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_RENDERER_IMPL_H_
#define MEDIA_FILTERS_RENDERER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioRenderer;
class Demuxer;
class TimeDeltaInterpolator;
class TimeSource;
class VideoRenderer;

class MEDIA_EXPORT RendererImpl : public Renderer {
 public:
  // Renders audio/video streams in |demuxer| using |audio_renderer| and
  // |video_renderer| provided. All methods except for GetMediaTime() run on the
  // |task_runner|. GetMediaTime() runs on the render main thread because it's
  // part of JS sync API. |get_duration_cb| is used to get the duration of the
  // stream.
  RendererImpl(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
               Demuxer* demuxer,
               scoped_ptr<AudioRenderer> audio_renderer,
               scoped_ptr<VideoRenderer> video_renderer);

  virtual ~RendererImpl();

  // Renderer implementation.
  virtual void Initialize(const base::Closure& init_cb,
                          const StatisticsCB& statistics_cb,
                          const base::Closure& ended_cb,
                          const PipelineStatusCB& error_cb,
                          const BufferingStateCB& buffering_state_cb,
                          const TimeDeltaCB& get_duration_cb) OVERRIDE;
  virtual void Flush(const base::Closure& flush_cb) OVERRIDE;
  virtual void StartPlayingFrom(base::TimeDelta time) OVERRIDE;
  virtual void SetPlaybackRate(float playback_rate) OVERRIDE;
  virtual void SetVolume(float volume) OVERRIDE;
  virtual base::TimeDelta GetMediaTime() OVERRIDE;
  virtual bool HasAudio() OVERRIDE;
  virtual bool HasVideo() OVERRIDE;
  virtual void SetCdm(MediaKeys* cdm) OVERRIDE;

  // Helper functions for testing purposes. Must be called before Initialize().
  void DisableUnderflowForTesting();
  void SetTimeDeltaInterpolatorForTesting(TimeDeltaInterpolator* interpolator);

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_FLUSHING,
    STATE_PLAYING,
    STATE_ERROR
  };

  base::TimeDelta GetMediaDuration();

  // Helper functions and callbacks for Initialize().
  void InitializeAudioRenderer();
  void OnAudioRendererInitializeDone(PipelineStatus status);
  void InitializeVideoRenderer();
  void OnVideoRendererInitializeDone(PipelineStatus status);

  // Helper functions and callbacks for Flush().
  void FlushAudioRenderer();
  void OnAudioRendererFlushDone();
  void FlushVideoRenderer();
  void OnVideoRendererFlushDone();

  // Callback executed by audio renderer to update clock time.
  void OnAudioTimeUpdate(base::TimeDelta time, base::TimeDelta max_time);

  // Callback executed by video renderer to update clock time.
  void OnVideoTimeUpdate(base::TimeDelta max_time);

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats);

  // Collection of callback methods and helpers for tracking changes in
  // buffering state and transition from paused/underflow states and playing
  // states.
  //
  // While in the kPlaying state:
  //   - A waiting to non-waiting transition indicates preroll has completed
  //     and StartPlayback() should be called
  //   - A non-waiting to waiting transition indicates underflow has occurred
  //     and PausePlayback() should be called
  void OnBufferingStateChanged(BufferingState* buffering_state,
                               BufferingState new_buffering_state);
  bool WaitingForEnoughData() const;
  void PausePlayback();
  void StartPlayback();

  void PauseClockAndStopTicking_Locked();
  void StartClockIfWaitingForTimeUpdate_Locked();

  // Callbacks executed when a renderer has ended.
  void OnAudioRendererEnded();
  void OnVideoRendererEnded();
  void RunEndedCallbackIfNeeded();

  // Callback executed when a runtime error happens.
  void OnError(PipelineStatus error);

  void FireAllPendingCallbacks();

  State state_;

  // Task runner used to execute pipeline tasks.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  Demuxer* demuxer_;

  // Permanent callback to get the media duration.
  TimeDeltaCB get_duration_cb_;

  // Permanent callbacks to notify various renderer states/stats.
  StatisticsCB statistics_cb_;
  base::Closure ended_cb_;
  PipelineStatusCB error_cb_;
  BufferingStateCB buffering_state_cb_;

  // Temporary callback used for Initialize() and Flush().
  base::Closure init_cb_;
  base::Closure flush_cb_;

  scoped_ptr<AudioRenderer> audio_renderer_;
  scoped_ptr<VideoRenderer> video_renderer_;

  // Renderer-provided time source used to control playback.
  TimeSource* time_source_;

  // The time to start playback from after starting/seeking has completed.
  base::TimeDelta start_time_;

  BufferingState audio_buffering_state_;
  BufferingState video_buffering_state_;

  // Whether we've received the audio/video ended events.
  bool audio_ended_;
  bool video_ended_;

  bool underflow_disabled_for_testing_;

  // Protects time interpolation related member variables, i.e. |interpolator_|,
  // |default_tick_clock_| and |interpolation_state_|. This is because
  // |interpolator_| can be used on different threads (see GetMediaTime()).
  mutable base::Lock interpolator_lock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  scoped_ptr<TimeDeltaInterpolator> interpolator_;

  // base::TickClock used by |interpolator_|.
  // TODO(xhwang): This can be TimeDeltaInterpolator's implementation detail.
  base::DefaultTickClock default_tick_clock_;

  enum InterpolationState {
    // Audio (if present) is not rendering. Time isn't being interpolated.
    INTERPOLATION_STOPPED,

    // Audio (if present) is rendering. Time isn't being interpolated.
    INTERPOLATION_WAITING_FOR_AUDIO_TIME_UPDATE,

    // Audio (if present) is rendering. Time is being interpolated.
    INTERPOLATION_STARTED,
  };

  InterpolationState interpolation_state_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<RendererImpl> weak_factory_;
  base::WeakPtr<RendererImpl> weak_this_;

  DISALLOW_COPY_AND_ASSIGN(RendererImpl);
};

}  // namespace media

#endif  // MEDIA_FILTERS_RENDERER_IMPL_H_
