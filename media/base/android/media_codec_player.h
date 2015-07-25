// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
#define MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_player_android.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/time_delta_interpolator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/android/scoped_java_surface.h"

// The MediaCodecPlayer class implements the media player by using Android's
// MediaCodec. It differs from MediaSourcePlayer in that it removes most
// processing away from the UI thread: it uses a dedicated Media thread to
// receive the data and to handle the commands.

// The player works as a state machine. Here are relationships between states:
//
//   [ Paused ] ------------------------                         (Any state)
//        |                             |                             |
//        |                             v                             v
//        | <------------------[ WaitingForConfig ]               [ Error ]
//        |
//        |
//        | <----------------------------------------------
//        v                                                |
//   [ Prefetching ] -------------------                   |
//        |                             |                  |
//        |                             v                  |
//        | <-----------------[ WaitingForSurface ]        |
//        v                                                |
//   [ Playing ]                                           |
//        |                                                |
//        |                                                |
//        v                                                |
//   [ Stopping ] --------------------------------> [ WaitingForSeek ]


//  Events and actions for pause/resume workflow.
//  ---------------------------------------------
//
//                                         Start, no config:
//    ------------------------> [ Paused ] -----------------> [   Waiting   ]
//    |  StopDone:                                            [ for configs ]
//    |                            ^  |                              /
//    |                            |  |                             /
//    |                    Pause:  |  | Start w/config:            /
//    |                            |  |    dec.Prefetch           /
//    |                            |  |                          /
//    |                            |  |                         /
//    |                            |  |                        /
//    |                            |  |                       / DemuxerConfigs:
//    |                            |  |                      /    dec.Prefetch
//    |                            |  |                     /
//    |                            |  |                    /
//    |                            |  v                   /
//    |                                                  /
//    |   ------------------> [ Prefetching ]  <--------/      [   Waiting   ]
//    |   |                   [             ] -------------->  [ for surface ]
//    |   |                          |         PrefetchDone,         /
//    |   |                          |          no surface:         /
//    |   |                          |                             /
//    |   |                          |                            /
//    |   | StopDone w/              |                           /
//    |   | pending start:           | PrefetchDone:            /
//    |   |    dec.Prefetch          |   dec.Start             /
//    |   |                          |                        / SetSurface:
//    |   |                          |                       /     dec.Start
//    |   |                          |                      /
//    |   |                          v                     /
//    |   |                                               /
//    |   |                     [ Playing ]   <----------/
//    |   |
//    |   |                          |
//    |   |                          |
//    |   |                          | Pause: dec.RequestToStop
//    |   |                          |
//    |   |                          |
//    |   |                          v
//    |   |
//    ------------------------- [ Stopping ]


//   Events and actions for seek workflow.
//   -------------------------------------
//
//                   Seek:                   --       --
//                       demuxer.RequestSeek |         |
//  [   Paused    ] -----------------------> |         |
//  [             ] <----------------------- |         |--
//                      SeekDone:            |         |  |
//                                           |         |  |
//                                           |         |  |
//                                           |         |  |
//                                           |         |  | Start:
//                    Seek:                  |         |  |   SetPendingStart
//                      dec.Stop             |         |  |
//                      SetPendingSeek       |         |  |
//                      demuxer.RequestSeek  |         |  |
//  [ Prefetching ] -----------------------> |         |  |
//  [             ] <----------------------  |         |  | Pause:
//        |          SeekDone                |         |  |   RemovePendingStart
//        |          w/pending start:        |         |  |
//        |            dec.Prefetch          | Waiting |  |
//        |                                  |   for   |  | Seek:
//        |                                  |   seek  |  |   SetPendingSeek
//        |                                  |         |  |
//        | PrefetchDone: dec.Start          |         |  |
//        |                                  |         |  | SeekDone
//        v                                  |         |  | w/pending seek:
//                                           |         |  | demuxer.RequestSeek
//  [   Playing   ]                          |         |  |
//                                           |         |  |
//        |                                  |         |<-
//        | Seek: SetPendingStart            |         |
//        |       SetPendingSeek             |         |
//        |       dec.RequestToStop          |         |
//        |                                  |         |
//        |                                  |         |
//        v                                  |         |
//                                           |         |
//  [  Stopping   ] -----------------------> |         |
//                  StopDone                 --       --
//                    w/pending seek:
//                      demuxer.RequestSeek

namespace media {

class BrowserCdm;
class MediaCodecAudioDecoder;
class MediaCodecVideoDecoder;

// Returns the task runner for the media thread
MEDIA_EXPORT scoped_refptr<base::SingleThreadTaskRunner> GetMediaTaskRunner();

class MEDIA_EXPORT MediaCodecPlayer : public MediaPlayerAndroid,
                                      public DemuxerAndroidClient {
 public:
  // Typedefs for the notification callbacks
  typedef base::Callback<void(base::TimeDelta, const gfx::Size&)>
      MetadataChangedCallback;

  typedef base::Callback<void(base::TimeDelta, base::TimeTicks)>
      TimeUpdateCallback;

  typedef base::Callback<void(const base::TimeDelta& current_timestamp)>
      SeekDoneCallback;

  // Constructs a player with the given ID and demuxer. |manager| must outlive
  // the lifetime of this object.
  MediaCodecPlayer(int player_id,
                   base::WeakPtr<MediaPlayerManager> manager,
                   const RequestMediaResourcesCB& request_media_resources_cb,
                   scoped_ptr<DemuxerAndroid> demuxer,
                   const GURL& frame_url);
  ~MediaCodecPlayer() override;

  // A helper method that performs the media thread part of initialization.
  void Initialize();

  // MediaPlayerAndroid implementation.
  void DeleteOnCorrectThread() override;
  void SetVideoSurface(gfx::ScopedJavaSurface surface) override;
  void Start() override;
  void Pause(bool is_media_related_action) override;
  void SeekTo(base::TimeDelta timestamp) override;
  void Release() override;
  void SetVolume(double volume) override;
  int GetVideoWidth() override;
  int GetVideoHeight() override;
  base::TimeDelta GetCurrentTime() override;
  base::TimeDelta GetDuration() override;
  bool IsPlaying() override;
  bool CanPause() override;
  bool CanSeekForward() override;
  bool CanSeekBackward() override;
  bool IsPlayerReady() override;
  void SetCdm(BrowserCdm* cdm) override;

  // DemuxerAndroidClient implementation.
  void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) override;
  void OnDemuxerDataAvailable(const DemuxerData& params) override;
  void OnDemuxerSeekDone(base::TimeDelta actual_browser_seek_time) override;
  void OnDemuxerDurationChanged(base::TimeDelta duration) override;

 private:
  // The state machine states.
  enum PlayerState {
    STATE_PAUSED,
    STATE_WAITING_FOR_CONFIG,
    STATE_PREFETCHING,
    STATE_PLAYING,
    STATE_STOPPING,
    STATE_WAITING_FOR_SURFACE,
    STATE_WAITING_FOR_SEEK,
    STATE_ERROR,
  };

  // Cached values for the manager.
  struct MediaMetadata {
    base::TimeDelta duration;
    gfx::Size video_size;
  };

  // Information about current seek in progress.
  struct SeekInfo {
    const base::TimeDelta seek_time;
    const bool is_browser_seek;
    SeekInfo(base::TimeDelta time, bool browser_seek)
        : seek_time(time), is_browser_seek(browser_seek) {}
  };

  // MediaPlayerAndroid implementation.
  // This method caches the data and calls manager's OnMediaMetadataChanged().
  void OnMediaMetadataChanged(base::TimeDelta duration,
                              const gfx::Size& video_size) override;

  // This method caches the current time and calls manager's OnTimeUpdate().
  void OnTimeUpdate(base::TimeDelta current_timestamp,
                    base::TimeTicks current_time_ticks) override;

  // Callbacks from decoders
  void RequestDemuxerData(DemuxerStream::Type stream_type);
  void OnPrefetchDone();
  void OnStopDone();
  void OnError();
  void OnStarvation(DemuxerStream::Type stream_type);
  void OnTimeIntervalUpdate(DemuxerStream::Type stream_type,
                            base::TimeDelta now_playing,
                            base::TimeDelta last_buffered);

  // Callbacks from video decoder
  void OnVideoCodecCreated();
  void OnVideoResolutionChanged(const gfx::Size& size);

  // Operations called from the state machine.
  void SetState(PlayerState new_state);
  void SetPendingSurface(gfx::ScopedJavaSurface surface);
  bool HasPendingSurface() const;
  void SetPendingStart(bool need_to_start);
  bool HasPendingStart() const;
  void SetPendingSeek(base::TimeDelta timestamp);
  base::TimeDelta GetPendingSeek() const;
  bool HasVideo() const;
  bool HasAudio() const;
  void SetDemuxerConfigs(const DemuxerConfigs& configs);
  void StartPrefetchDecoders();
  void StartPlaybackDecoders();
  void StopDecoders();
  void RequestToStopDecoders();
  void RequestDemuxerSeek(base::TimeDelta seek_time,
                          bool is_browser_seek = false);
  void ReleaseDecoderResources();

  // Helper methods.
  void CreateDecoders();
  bool AudioFinished() const;
  bool VideoFinished() const;
  base::TimeDelta GetInterpolatedTime();

  static const char* AsString(PlayerState state);

  // Data.

  // Object for posting tasks on UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  // Major components: demuxer, audio and video decoders.
  scoped_ptr<DemuxerAndroid> demuxer_;
  scoped_ptr<MediaCodecAudioDecoder> audio_decoder_;
  scoped_ptr<MediaCodecVideoDecoder> video_decoder_;

  // The state of the state machine.
  PlayerState state_;

  // Notification callbacks, they call MediaPlayerManager.
  base::Closure request_resources_cb_;
  TimeUpdateCallback time_update_cb_;
  base::Closure completion_cb_;
  SeekDoneCallback seek_done_cb_;

  // A callback that updates metadata cache and calls the manager.
  MetadataChangedCallback metadata_changed_cb_;

  // We call the base class' AttachListener() and DetachListener() methods on UI
  // thread with these callbacks.
  base::Closure attach_listener_cb_;
  base::Closure detach_listener_cb_;

  // Error callback is posted by decoders or by this class itself if we cannot
  // configure or start decoder.
  base::Closure error_cb_;

  // Total duration reported by demuxer.
  base::TimeDelta duration_;

  // base::TickClock used by |interpolator_|.
  base::DefaultTickClock default_tick_clock_;

  // Tracks the most recent media time update and provides interpolated values
  // as playback progresses.
  TimeDeltaInterpolator interpolator_;

  // Pending data to be picked up by the upcoming state.
  gfx::ScopedJavaSurface pending_surface_;
  bool pending_start_;
  base::TimeDelta pending_seek_;

  // Data associated with a seek in progress.
  scoped_ptr<SeekInfo> seek_info_;

  // Configuration data for the manager, accessed on the UI thread.
  MediaMetadata metadata_cache_;

  // Cached current time, accessed on UI thread.
  base::TimeDelta current_time_cache_;

  base::WeakPtr<MediaCodecPlayer> media_weak_this_;
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaCodecPlayer> media_weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecPlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CODEC_PLAYER_H_
