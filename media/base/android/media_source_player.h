// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_
#define MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_

#include <jni.h>
#include <map>
#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_android.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_android.h"
#include "media/base/clock.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderJob;
class AudioTimestampHelper;
class VideoDecoderJob;

// This class handles media source extensions on Android. It uses Android
// MediaCodec to decode audio and video streams in two separate threads.
class MEDIA_EXPORT MediaSourcePlayer : public MediaPlayerAndroid,
                                       public DemuxerAndroidClient {
 public:
  // Constructs a player with the given ID and demuxer. |manager| must outlive
  // the lifetime of this object.
  MediaSourcePlayer(int player_id,
                    MediaPlayerManager* manager,
                    const RequestMediaResourcesCB& request_media_resources_cb,
                    const ReleaseMediaResourcesCB& release_media_resources_cb,
                    scoped_ptr<DemuxerAndroid> demuxer);
  virtual ~MediaSourcePlayer();

  // MediaPlayerAndroid implementation.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Pause(bool is_media_related_action ALLOW_UNUSED) OVERRIDE;
  virtual void SeekTo(base::TimeDelta timestamp) OVERRIDE;
  virtual void Release() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual int GetVideoWidth() OVERRIDE;
  virtual int GetVideoHeight() OVERRIDE;
  virtual base::TimeDelta GetCurrentTime() OVERRIDE;
  virtual base::TimeDelta GetDuration() OVERRIDE;
  virtual bool IsPlaying() OVERRIDE;
  virtual bool CanPause() OVERRIDE;
  virtual bool CanSeekForward() OVERRIDE;
  virtual bool CanSeekBackward() OVERRIDE;
  virtual bool IsPlayerReady() OVERRIDE;
  virtual void SetDrmBridge(MediaDrmBridge* drm_bridge) OVERRIDE;
  virtual void OnKeyAdded() OVERRIDE;
  virtual bool IsSurfaceInUse() const OVERRIDE;

  // DemuxerAndroidClient implementation.
  virtual void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) OVERRIDE;
  virtual void OnDemuxerDataAvailable(const DemuxerData& params) OVERRIDE;
  virtual void OnDemuxerSeekDone(
      base::TimeDelta actual_browser_seek_time) OVERRIDE;
  virtual void OnDemuxerDurationChanged(base::TimeDelta duration) OVERRIDE;

 private:
  friend class MediaSourcePlayerTest;

  // Update the current timestamp.
  void UpdateTimestamps(base::TimeDelta presentation_timestamp,
                        size_t audio_output_bytes);

  // Helper function for starting media playback.
  void StartInternal();

  // Playback is completed for one channel.
  void PlaybackCompleted(bool is_audio);

  // Called when the decoder finishes its task.
  void MediaDecoderCallback(
        bool is_audio, MediaCodecStatus status,
        base::TimeDelta presentation_timestamp,
        size_t audio_output_bytes);

  // Gets MediaCrypto object from |drm_bridge_|.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Callback to notify that MediaCrypto is ready in |drm_bridge_|.
  void OnMediaCryptoReady();

  // Handle pending events if all the decoder jobs are not currently decoding.
  void ProcessPendingEvents();

  // Helper method to clear any pending |SURFACE_CHANGE_EVENT_PENDING|
  // and reset |video_decoder_job_| to null.
  void ResetVideoDecoderJob();
  void ResetAudioDecoderJob();

  // Helper methods to configure the decoder jobs.
  void ConfigureVideoDecoderJob();
  void ConfigureAudioDecoderJob();

  // Flush the decoders and clean up all the data needs to be decoded.
  void ClearDecodingData();

  // Called to decode more data.
  void DecodeMoreAudio();
  void DecodeMoreVideo();

  // Functions check whether audio/video is present.
  bool HasVideo();
  bool HasAudio();

  // Functions that check whether audio/video stream has reached end of output
  // or are not present in player configuration.
  bool AudioFinished();
  bool VideoFinished();

  // Determine seekability based on duration.
  bool Seekable();

  // Called when the |decoder_starvation_callback_| times out.
  void OnDecoderStarved();

  // Starts the |decoder_starvation_callback_| task with the timeout value.
  // |presentation_timestamp| - The presentation timestamp used for starvation
  // timeout computations. It represents the timestamp of the last piece of
  // decoded data.
  void StartStarvationCallback(base::TimeDelta presentation_timestamp);

  // Schedules a seek event in |pending_events_| and calls StopDecode() on all
  // the MediaDecoderJobs. Sets clock to |seek_time|, and resets
  // |pending_seek_|. There must not already be a seek event in
  // |pending_events_|.
  void ScheduleSeekEventAndStopDecoding(base::TimeDelta seek_time);

  // Schedules a browser seek event. We must not currently be processing any
  // seek. Note that there is possibility that browser seek of renderer demuxer
  // may unexpectedly stall due to lack of buffered data at or after the browser
  // seek time.
  // TODO(wolenetz): Instead of doing hack browser seek, replay cached data
  // since last keyframe. See http://crbug.com/304234.
  void BrowserSeekToCurrentTime();

  // Helper function to set the volume.
  void SetVolumeInternal();

  // Helper function to determine whether a protected surface is needed for
  // video playback.
  bool IsProtectedSurfaceRequired();

  // Called when a MediaDecoderJob finishes prefetching data. Once all
  // MediaDecoderJobs have prefetched data, then this method updates
  // |start_time_ticks_| and |start_presentation_timestamp_| so that video can
  // resync with audio and starts decoding.
  void OnPrefetchDone();

  // Test-only method to setup hook for the completion of the next decode cycle.
  // This callback state is cleared when it is next run.
  // Prevent usage creep by only calling this from the
  // ReleaseWithOnPrefetchDoneAlreadyPosted MediaSourcePlayerTest.
  void set_decode_callback_for_testing(const base::Closure& test_decode_cb) {
    decode_callback_for_testing_ = test_decode_cb;
  }

  // TODO(qinmin/wolenetz): Reorder these based on their priority from
  // ProcessPendingEvents(). Release() and other routines are dependent upon
  // priority consistency.
  enum PendingEventFlags {
    NO_EVENT_PENDING = 0,
    SEEK_EVENT_PENDING = 1 << 0,
    SURFACE_CHANGE_EVENT_PENDING = 1 << 1,
    CONFIG_CHANGE_EVENT_PENDING = 1 << 2,
    PREFETCH_REQUEST_EVENT_PENDING = 1 << 3,
    PREFETCH_DONE_EVENT_PENDING = 1 << 4,
  };

  static const char* GetEventName(PendingEventFlags event);
  bool IsEventPending(PendingEventFlags event) const;
  void SetPendingEvent(PendingEventFlags event);
  void ClearPendingEvent(PendingEventFlags event);

  scoped_ptr<DemuxerAndroid> demuxer_;

  // Pending event that the player needs to do.
  unsigned pending_event_;

  // Stats about the media.
  base::TimeDelta duration_;
  int width_;
  int height_;
  AudioCodec audio_codec_;
  VideoCodec video_codec_;
  int num_channels_;
  int sampling_rate_;
  // TODO(xhwang/qinmin): Add |video_extra_data_|.
  std::vector<uint8> audio_extra_data_;
  bool reached_audio_eos_;
  bool reached_video_eos_;
  bool playing_;
  bool is_audio_encrypted_;
  bool is_video_encrypted_;
  double volume_;

  // base::TickClock used by |clock_|.
  base::DefaultTickClock default_tick_clock_;

  // Reference clock. Keeps track of current playback time.
  Clock clock_;

  // Timestamps for providing simple A/V sync. When start decoding an audio
  // chunk, we record its presentation timestamp and the current system time.
  // Then we use this information to estimate when the next audio/video frame
  // should be rendered.
  // TODO(qinmin): Need to fix the problem if audio/video lagged too far behind
  // due to network or decoding problem.
  base::TimeTicks start_time_ticks_;
  base::TimeDelta start_presentation_timestamp_;

  // The surface object currently owned by the player.
  gfx::ScopedJavaSurface surface_;

  // Track whether or not the player has received any video data since the most
  // recent of player construction, end of last seek, or receiving and
  // detecting a |kConfigChanged| access unit from the demuxer.
  // If no such video data has been received, the next video data begins with
  // an I-frame. Otherwise, we have no such guarantee.
  bool next_video_data_is_iframe_;

  // Flag that is true if doing a hack browser seek or false if doing a
  // regular seek. Only valid when |SEEK_EVENT_PENDING| is pending.
  // TODO(wolenetz): Instead of doing hack browser seek, replay cached data
  // since last keyframe. See http://crbug.com/304234.
  bool doing_browser_seek_;

  // If already doing a browser seek when a regular seek request arrives,
  // these fields remember the regular seek so OnDemuxerSeekDone() can trigger
  // it when the browser seek is done. These are only valid when
  // |SEEK_EVENT_PENDING| is pending.
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Decoder jobs.
  scoped_ptr<AudioDecoderJob, MediaDecoderJob::Deleter> audio_decoder_job_;
  scoped_ptr<VideoDecoderJob, MediaDecoderJob::Deleter> video_decoder_job_;

  bool reconfig_audio_decoder_;
  bool reconfig_video_decoder_;

  // Track the most recent preroll target. Decoder re-creation needs this to
  // resume any in-progress preroll.
  base::TimeDelta preroll_timestamp_;

  // A cancelable task that is posted when the audio decoder starts requesting
  // new data. This callback runs if no data arrives before the timeout period
  // elapses.
  base::CancelableClosure decoder_starvation_callback_;

  // Object to calculate the current audio timestamp for A/V sync.
  scoped_ptr<AudioTimestampHelper> audio_timestamp_helper_;

  MediaDrmBridge* drm_bridge_;

  // No decryption key available to decrypt the encrypted buffer. In this case,
  // the player should pause. When a new key is added (OnKeyAdded()), we should
  // try to start playback again.
  bool is_waiting_for_key_;

  // Test-only callback for hooking the completion of the next decode cycle.
  base::Closure decode_callback_for_testing_;

  // Whether |surface_| is currently used by the player.
  bool is_surface_in_use_;

  // Whether there are pending data requests by the decoder.
  bool has_pending_audio_data_request_;
  bool has_pending_video_data_request_;

  // Weak pointer passed to media decoder jobs for callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaSourcePlayer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_
