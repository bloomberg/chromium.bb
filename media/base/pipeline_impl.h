// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_PIPELINE_IMPL_H_
#define MEDIA_BASE_PIPELINE_IMPL_H_

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/cdm_context.h"
#include "media/base/demuxer.h"
#include "media/base/media_export.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/serial_runner.h"
#include "media/base/text_track.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class MediaLog;
class Renderer;
class TextRenderer;

// Pipeline runs the media pipeline.  Filters are created and called on the
// task runner injected into this object. Pipeline works like a state
// machine to perform asynchronous initialization, pausing, seeking and playing.
//
// Here's a state diagram that describes the lifetime of this object.
//
//   [ *Created ]                       [ Any State ]
//         | Start()                         | Stop() / SetError()
//         V                                 V
//   [ InitXXX (for each filter) ]      [ Stopping ]
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
// If any error ever happens, this object will transition to the "Error" state
// from any state. If Stop() is ever called, this object will transition to
// "Stopped" state.
//
// TODO(sandersd): It should be possible to pass through Suspended when going
// from InitDemuxer to InitRenderer, thereby eliminating the Resuming state.
// Some annoying differences between the two paths need to be removed first.
class MEDIA_EXPORT PipelineImpl : public Pipeline, public DemuxerHost {
 public:
  // Constructs a media pipeline that will execute media tasks on
  // |media_task_runner|.
  PipelineImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      MediaLog* media_log);
  ~PipelineImpl() override;

  void SetErrorForTesting(PipelineStatus status);
  bool HasWeakPtrsForTesting() const;

  // Pipeline implementation.
  void Start(Demuxer* demuxer,
             std::unique_ptr<Renderer> renderer,
             Client* client,
             const PipelineStatusCB& seek_cb) override;
  void Stop() override;
  void Seek(base::TimeDelta time, const PipelineStatusCB& seek_cb) override;
  bool IsRunning() const override;
  double GetPlaybackRate() const override;
  void SetPlaybackRate(double playback_rate) override;
  void Suspend(const PipelineStatusCB& suspend_cb) override;
  void Resume(std::unique_ptr<Renderer> renderer,
              base::TimeDelta timestamp,
              const PipelineStatusCB& seek_cb) override;
  float GetVolume() const override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() const override;
  Ranges<base::TimeDelta> GetBufferedTimeRanges() const override;
  base::TimeDelta GetMediaDuration() const override;
  bool DidLoadingProgress() override;
  PipelineStatistics GetStatistics() const override;
  void SetCdm(CdmContext* cdm_context,
              const CdmAttachedCB& cdm_attached_cb) override;

 private:
  friend class MediaLog;
  friend class PipelineImplTest;

  // Pipeline states, as described above.
  enum State {
    kCreated,
    kInitDemuxer,
    kInitRenderer,
    kSeeking,
    kPlaying,
    kStopping,
    kStopped,
    kSuspending,
    kSuspended,
    kResuming,
  };

  // Updates |state_|. All state transitions should use this call.
  void SetState(State next_state);

  static const char* GetStateString(State state);
  State GetNextState() const;

  // Helper method that runs & resets |seek_cb_| and resets |seek_timestamp_|
  // and |seek_pending_|.
  void FinishSeek();

  // DemuxerHost implementaion.
  void OnBufferedTimeRangesChanged(
      const Ranges<base::TimeDelta>& ranges) override;
  void SetDuration(base::TimeDelta duration) override;
  void OnDemuxerError(PipelineStatus error) override;
  void AddTextStream(DemuxerStream* text_stream,
                     const TextTrackConfig& config) override;
  void RemoveTextStream(DemuxerStream* text_stream) override;

  // Callback executed when a rendering error happened, initiating the teardown
  // sequence.
  void OnError(PipelineStatus error);

  // Callback executed by filters to update statistics.
  void OnUpdateStatistics(const PipelineStatistics& stats_delta);

  // Callback executed by renderer when waiting for decryption key.
  void OnWaitingForDecryptionKey();

  // The following "task" methods correspond to the public methods, but these
  // methods are run as the result of posting a task to the Pipeline's
  // task runner.
  void StartTask();

  // Suspends the pipeline, discarding the current renderer.
  void SuspendTask(const PipelineStatusCB& suspend_cb);

  // Resumes the pipeline with a new renderer, and initializes it with a seek.
  void ResumeTask(std::unique_ptr<Renderer> renderer,
                  base::TimeDelta timestamp,
                  const PipelineStatusCB& seek_sb);

  // Stops and destroys all filters, placing the pipeline in the kStopped state.
  void StopTask(const base::Closure& stop_cb);

  // Carries out stopping and destroying all filters, placing the pipeline in
  // the kStopped state.
  void ErrorChangedTask(PipelineStatus error);

  // Carries out notifying filters that the playback rate has changed.
  void PlaybackRateChangedTask(double playback_rate);

  // Carries out notifying filters that the volume has changed.
  void VolumeChangedTask(float volume);

  // Carries out notifying filters that we are seeking to a new timestamp.
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& seek_cb);

  // Carries out setting the |cdm_context| in |renderer_|, and then fires
  // |cdm_attached_cb| with the result. If |renderer_| is null,
  // |cdm_attached_cb| will be fired immediately with true, and |cdm_context|
  // will be set in |renderer_| later when |renderer_| is created.
  void SetCdmTask(CdmContext* cdm_context,
                  const CdmAttachedCB& cdm_attached_cb);
  void OnCdmAttached(const CdmAttachedCB& cdm_attached_cb,
                     CdmContext* cdm_context,
                     bool success);

  // Callbacks executed when a renderer has ended.
  void OnRendererEnded();
  void OnTextRendererEnded();
  void RunEndedCallbackIfNeeded();

  std::unique_ptr<TextRenderer> CreateTextRenderer();

  // Carries out adding a new text stream to the text renderer.
  void AddTextStreamTask(DemuxerStream* text_stream,
                         const TextTrackConfig& config);

  // Carries out removing a text stream from the text renderer.
  void RemoveTextStreamTask(DemuxerStream* text_stream);

  // Callbacks executed when a text track is added.
  void OnAddTextTrack(const TextTrackConfig& config,
                      const AddTextTrackDoneCB& done_cb);

  // Kicks off initialization for each media object, executing |done_cb| with
  // the result when completed.
  void InitializeDemuxer(const PipelineStatusCB& done_cb);
  void InitializeRenderer(const PipelineStatusCB& done_cb);

  void StateTransitionTask(PipelineStatus status);

  // Initiates an asynchronous pause-flush-seek-preroll call sequence
  // executing |done_cb| with the final status when completed.
  void DoSeek(base::TimeDelta seek_timestamp, const PipelineStatusCB& done_cb);

  // Stops media rendering and executes |stop_cb_| when done.
  void DoStop();

  void ReportMetadata();

  void BufferingStateChanged(BufferingState new_buffering_state);

  // Task runner of the thread on which this class is constructed.
  // Also used to post notifications on Pipeline::Client object.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Task runner used to execute pipeline tasks.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // MediaLog to which to log events.
  scoped_refptr<MediaLog> media_log_;

  // Lock used to serialize access for the following data members.
  mutable base::Lock lock_;

  // Whether or not the pipeline is running.
  bool running_;

  // Amount of available buffered data as reported by |demuxer_|.
  Ranges<base::TimeDelta> buffered_time_ranges_;

  // True when OnBufferedTimeRangesChanged() has been called more recently than
  // DidLoadingProgress().
  bool did_loading_progress_;

  // Current volume level (from 0.0f to 1.0f).  This value is set immediately
  // via SetVolume() and a task is dispatched on the task runner to notify the
  // filters.
  float volume_;

  // Current playback rate (>= 0.0).  This value is set immediately via
  // SetPlaybackRate() and a task is dispatched on the task runner to notify
  // the filters.
  double playback_rate_;

  // Current duration as reported by |demuxer_|.
  base::TimeDelta duration_;

  // Status of the pipeline.  Initialized to PIPELINE_OK which indicates that
  // the pipeline is operating correctly. Any other value indicates that the
  // pipeline is stopped or is stopping.  Clients can call the Stop() method to
  // reset the pipeline state, and restore this to PIPELINE_OK.
  PipelineStatus status_;

  // The following data members are only accessed by tasks posted to
  // |task_runner_|.

  // Member that tracks the current state.
  State state_;

  // The timestamp to start playback from after starting/seeking/resuming has
  // completed.
  base::TimeDelta start_timestamp_;

  // The media timestamp to return while the pipeline is suspended. Otherwise
  // set to kNoTimestamp().
  base::TimeDelta suspend_timestamp_;

  // Whether we've received the audio/video/text ended events.
  bool renderer_ended_;
  bool text_renderer_ended_;

  // Temporary callback used for Start(), Seek(), and Resume().
  PipelineStatusCB seek_cb_;

  // Temporary callback used for Stop().
  base::Closure stop_cb_;

  // Temporary callback used for Suspend().
  PipelineStatusCB suspend_cb_;

  // Holds the initialized demuxer. Used for seeking. Owned by client.
  Demuxer* demuxer_;

  // Holds the initialized renderers. Used for setting the volume,
  // playback rate, and determining when playback has finished.
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<TextRenderer> text_renderer_;

  // Holds the client passed on Start().
  // Initialized, Dereferenced, and Invalidated on |main_task_runner_|.
  // Used on |media_task_runner_| to post tasks on |main_task_runner_|.
  base::WeakPtr<Client> weak_client_;
  // Created and destroyed on |main_task_runner_|.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_weak_factory_;

  PipelineStatistics statistics_;

  std::unique_ptr<SerialRunner> pending_callbacks_;

  // The CdmContext to be used to decrypt (and decode) encrypted stream in this
  // pipeline. It is set when SetCdm() succeeds on the renderer (or when
  // SetCdm() is called before Start()), after which it is guaranteed to outlive
  // this pipeline. The saved value will be used to configure new renderers,
  // when starting or resuming.
  CdmContext* cdm_context_;

  base::ThreadChecker thread_checker_;

  // A weak pointer that can be safely copied on the media thread.
  base::WeakPtr<PipelineImpl> weak_this_;

  // Weak pointers must be created on the main thread, and must be dereferenced
  // on the media thread.
  //
  // Declared last so that weak pointers will be invalidated before all other
  // member variables.
  base::WeakPtrFactory<PipelineImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PipelineImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_PIPELINE_IMPL_H_
