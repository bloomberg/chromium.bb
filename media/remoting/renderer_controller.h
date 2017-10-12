// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_RENDERER_CONTROLLER_H_
#define MEDIA_REMOTING_RENDERER_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "build/buildflag.h"
#include "media/base/media_observer.h"
#include "media/media_features.h"
#include "media/remoting/metrics.h"
#include "media/remoting/shared_session.h"

namespace base {
class TickClock;
}

namespace media {

namespace remoting {

class RpcBroker;

// This class:
// 1) Implements the SharedSession::Client;
// 2) Monitors player events as a MediaObserver;
// 3) May trigger the switch of the media renderer between local playback
// and remoting.
class RendererController final : public SharedSession::Client,
                                 public MediaObserver {
 public:
  explicit RendererController(scoped_refptr<SharedSession> session);
  ~RendererController() override;

  // SharedSession::Client implementation.
  void OnStarted(bool success) override;
  void OnSessionStateChanged() override;

  // MediaObserver implementation.
  void OnBecameDominantVisibleContent(bool is_dominant) override;
  void OnSetCdm(CdmContext* cdm_context) override;
  void OnMetadataChanged(const PipelineMetadata& metadata) override;
  void OnRemotePlaybackDisabled(bool disabled) override;
  void OnPlaying() override;
  void OnPaused() override;
  void OnDataSourceInitialized(const GURL& url_after_redirects) override;
  void SetClient(MediaObserverClient* client) override;

  base::WeakPtr<RendererController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Used by AdaptiveRendererFactory to query whether to create a Media
  // Remoting Renderer.
  bool remote_rendering_started() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return remote_rendering_started_;
  }

  void StartDataPipe(std::unique_ptr<mojo::DataPipe> audio_data_pipe,
                     std::unique_ptr<mojo::DataPipe> video_data_pipe,
                     const SharedSession::DataPipeStartCallback& done_callback);

  // Used by CourierRenderer to query the session state.
  SharedSession* session() const { return session_.get(); }

#if BUILDFLAG(ENABLE_MEDIA_REMOTING_RPC)
  base::WeakPtr<RpcBroker> GetRpcBroker() const;
#endif

  // Called by CourierRenderer when it encountered a fatal error. This will
  // cause remoting to shut down and never start back up for the lifetime of
  // this controller.
  void OnRendererFatalError(StopTrigger stop_trigger);

 private:
  friend class RendererControllerTest;

  bool has_audio() const {
    return pipeline_metadata_.has_audio &&
           pipeline_metadata_.audio_decoder_config.IsValidConfig();
  }

  bool has_video() const {
    return pipeline_metadata_.has_video &&
           pipeline_metadata_.video_decoder_config.IsValidConfig();
  }

  // Called when the session availability state may have changed. Each call to
  // this method could cause a remoting session to be started or stopped; and if
  // that happens, the |start_trigger| or |stop_trigger| must be the reason.
  void UpdateFromSessionState(StartTrigger start_trigger,
                              StopTrigger stop_trigger);

  bool IsVideoCodecSupported() const;
  bool IsAudioCodecSupported() const;
  bool IsRemoteSinkAvailable() const;
  bool IsAudioOrVideoSupported() const;

  // Returns true if all of the technical requirements for the media pipeline
  // and remote rendering are being met. This does not include environmental
  // conditions, such as the content being dominant in the viewport, available
  // network bandwidth, etc.
  bool CanBeRemoting() const;

  // Determines whether to enter or leave Remoting mode and switches if
  // necessary. Each call to this method could cause a remoting session to be
  // started or stopped; and if that happens, the |start_trigger| or
  // |stop_trigger| must be the reason.
  void UpdateAndMaybeSwitch(StartTrigger start_trigger,
                            StopTrigger stop_trigger);

  // Activates or deactivates the remote playback monitoring based on whether
  // the element is compatible with Remote Playback API.
  void UpdateRemotePlaybackAvailabilityMonitoringState();

  // Start |delayed_start_stability_timer_| to ensure all preconditions are met
  // and held stable for a short time before starting remoting.
  void WaitForStabilityBeforeStart(StartTrigger start_trigger);
  // Cancel the start of remoting.
  void CancelDelayedStart();
  // Called when the delayed start ends. |decoded_bytes_before_delay| is the
  // total number of audio and video bytes decoded before the delayed start
  // began. |delayed_start_time| is the time that the delayed start began.
  void OnDelayedStartTimerFired(StartTrigger start_trigger,
                                size_t decoded_bytes_before_delay,
                                unsigned decoded_frame_count_before_delay,
                                base::TimeTicks delayed_start_time);

  // Helper to request the media pipeline switch to the remoting renderer.
  void StartRemoting(StartTrigger start_trigger);

  // Callback to get the estimated transmission capacity from Remoter.
  void OnReceivedTransmissionCapacity(double rate);

  // Indicates whether remoting is started.
  bool remote_rendering_started_ = false;

  // Indicates whether audio or video is encrypted.
  bool is_encrypted_ = false;

  // Indicates whether remote playback is currently disabled. This starts out as
  // true, and should be updated at least once via a call to
  // OnRemotePlaybackDisabled() at some point in the future. A web page
  // typically sets/removes the disableRemotePlayback attribute on a
  // HTMLMediaElement to disable/enable remoting of its content. Please see the
  // Remote Playback API spec for more details:
  // https://w3c.github.io/remote-playback
  bool is_remote_playback_disabled_ = true;

  // Indicates whether video is the dominant visible content in the tab.
  bool is_dominant_content_ = false;

  // Indicates whether video is paused.
  bool is_paused_ = true;

  // Indicates whether OnRendererFatalError() has been called. This indicates
  // one of several possible problems: 1) An environmental problem such as
  // out-of-memory, insufficient network bandwidth, etc. 2) The receiver may
  // have been unable to play-out the content correctly (e.g., not capable of a
  // high frame rate at a high resolution). 3) An implementation bug. In any
  // case, once a renderer encounters a fatal error, remoting will be shut down
  // and never start again for the lifetime of this controller.
  bool encountered_renderer_fatal_error_ = false;

  // This is initially the SharedSession passed to the ctor, and might be
  // replaced with a different instance later if OnSetCdm() is called.
  scoped_refptr<SharedSession> session_;

  // This is used to check all the methods are called on the current thread in
  // debug builds.
  base::ThreadChecker thread_checker_;

  // Current pipeline metadata.
  PipelineMetadata pipeline_metadata_;

  // Current data source information.
  GURL url_after_redirects_;

  // Records session events of interest.
  SessionMetricsRecorder metrics_recorder_;

  // Not owned by this class. Can only be set once by calling SetClient().
  MediaObserverClient* client_ = nullptr;

  // When this is running, it indicates that remoting will be started later
  // when the timer gets fired. The start will be canceled if there is any
  // precondition change that does not allow for remoting duting this period.
  // TODO(xjz): Estimate whether the transmission bandwidth is sufficient to
  // remote the content while this timer is running.
  base::OneShotTimer delayed_start_stability_timer_;

  std::unique_ptr<base::TickClock> clock_;

  // The estimated transmission capacity (bytes/s) from Remoter.
  double transmission_capacity_ = 0;

  base::WeakPtrFactory<RendererController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RendererController);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_RENDERER_CONTROLLER_H_
