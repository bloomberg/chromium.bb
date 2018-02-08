// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/media_export.h"
#include "media/base/pipeline.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaLog;

// Pipeline runs the media pipeline.  Filters are created and called on the
// task runner injected into this object. Pipeline works like a state
// machine to perform asynchronous initialization, pausing, seeking and playing.
//
// Here's a state diagram that describes the lifetime of this object.
//
//   [ *Created ]                       [ Any State ]
//         | Start()                         | Stop()
//         V                                 V
//   [ Starting ]                       [ Stopping ]
//         |                                 |
//         V                                 V
//   [ Playing ] <---------.            [ Stopped ]
//     |     | Seek()      |
//     |     V             |
//     |   [ Seeking ] ----'
//     |                   ^
//     | Suspend()         |
//     V                   |
//   [ Suspending ]        |
//     |                   |
//     V                   |
//   [ Suspended ]         |
//     | Resume()          |
//     V                   |
//   [ Resuming ] ---------'
//
// Initialization is a series of state transitions from "Created" through each
// filter initialization state.  When all filter initialization states have
// completed, we simulate a Seek() to the beginning of the media to give filters
// a chance to preroll. From then on the normal Seek() transitions are carried
// out and we start playing the media.
//
// If Stop() is ever called, this object will transition to "Stopped" state.
// Pipeline::Stop() is never called from withing PipelineImpl. It's |client_|'s
// responsibility to call stop when appropriate.
//
// TODO(sandersd): It should be possible to pass through Suspended when going
// from InitDemuxer to InitRenderer, thereby eliminating the Resuming state.
// Some annoying differences between the two paths need to be removed first.
class MEDIA_EXPORT PipelineImpl : public Pipeline {
 public:
  // Constructs a media pipeline that will execute media tasks on
  // |media_task_runner|.
  PipelineImpl(scoped_refptr<base::SingleThreadTaskRunner> media_task_runner,
               scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
               MediaLog* media_log);
  ~PipelineImpl() override;

  // Pipeline implementation.
  void Start(StartType start_type,
             Demuxer* demuxer,
             std::unique_ptr<Renderer> renderer,
             Client* client,
             const PipelineStatusCB& seek_cb) override;
  void Stop() override;
  void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) override;
  void Suspend(const PipelineStatusCB& suspend_cb) override;
  void Resume(std::unique_ptr<Renderer> renderer,
              base::TimeDelta time,
              const PipelineStatusCB& seek_cb) override;
  bool IsRunning() const override;
  bool IsSuspended() const override;
  double GetPlaybackRate() const override;
  void SetPlaybackRate(double playback_rate) override;
  float GetVolume() const override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() const override;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const override;
  base::TimeDelta GetMediaDuration() const override;
  bool DidLoadingProgress() override;
  PipelineStatistics GetStatistics() const override;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) override;

  // |enabled_track_ids| contains track ids of enabled audio tracks.
  void OnEnabledAudioTracksChanged(
      const std::vector<MediaTrack::Id>& enabled_track_ids) override;

  // |selected_track_id| is either empty, which means no video track is
  // selected, or contains the selected video track id.
  void OnSelectedVideoTrackChanged(
      base::Optional<MediaTrack::Id> selected_track_id) override;

 private:
  friend class MediaLog;
  class RendererWrapper;

  // Pipeline states, as described above.
  // TODO(alokp): Move this to RendererWrapper after removing the references
  // from MediaLog.
  enum State {
    kCreated,
    kStarting,
    kSeeking,
    kPlaying,
    kStopping,
    kStopped,
    kSuspending,
    kSuspended,
    kResuming,
  };
  static const char* GetStateString(State state);

  // Notifications from RendererWrapper.
  void OnError(PipelineStatus error);
  void OnEnded();
  void OnMetadata(PipelineMetadata metadata);
  void OnBufferingStateChange(BufferingState state);
  void OnDurationChange(base::TimeDelta duration);
  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb);
  void OnWaitingForDecryptionKey();
  void OnAudioConfigChange(const AudioDecoderConfig& config);
  void OnVideoConfigChange(const VideoDecoderConfig& config);
  void OnVideoNaturalSizeChange(const gfx::Size& size);
  void OnVideoOpacityChange(bool opaque);
  void OnVideoAverageKeyframeDistanceUpdate();
  void OnAudioDecoderChange(const std::string& name);
  void OnVideoDecoderChange(const std::string& name);

  // Task completion callbacks from RendererWrapper.
  void OnSeekDone(bool is_suspended);
  void OnSuspendDone();

  // Parameters passed in the constructor.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  MediaLog* const media_log_;

  // Pipeline client. Valid only while the pipeline is running.
  Client* client_;

  // RendererWrapper instance that runs on the media thread.
  std::unique_ptr<RendererWrapper> renderer_wrapper_;

  // Temporary callback used for Start(), Seek(), and Resume().
  PipelineStatusCB seek_cb_;

  // Temporary callback used for Suspend().
  PipelineStatusCB suspend_cb_;

  // Current playback rate (>= 0.0). This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the task runner to notify
  // the filters.
  double playback_rate_;

  // Current volume level (from 0.0f to 1.0f). This value is set immediately
  // via SetVolume() and a task is dispatched on the task runner to notify the
  // filters.
  float volume_;

  // Current duration as reported by Demuxer.
  base::TimeDelta duration_;

  // Set by GetMediaTime(), used to prevent the current media time value as
  // reported to JavaScript from going backwards in time.
  mutable base::TimeDelta last_media_time_;

  // Set by Seek(), used in place of asking the renderer for current media time
  // while a seek is pending. Renderer's time cannot be trusted until the seek
  // has completed.
  base::TimeDelta seek_time_;

  // Cached suspension state for the RendererWrapper.
  bool is_suspended_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<PipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
