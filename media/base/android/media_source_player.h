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
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_decoder_job.h"
#include "media/base/android/media_player_android.h"
#include "media/base/clock.h"
#include "media/base/media_export.h"

namespace media {

class AudioDecoderJob;
class AudioTimestampHelper;
class VideoDecoderJob;

// This class handles media source extensions on Android. It uses Android
// MediaCodec to decode audio and video streams in two separate threads.
// IPC is being used to send data from the render process to this object.
// TODO(qinmin): use shared memory to send data between processes.
class MEDIA_EXPORT MediaSourcePlayer : public MediaPlayerAndroid {
 public:
  // Construct a MediaSourcePlayer object with all the needed media player
  // callbacks.
  MediaSourcePlayer(int player_id, MediaPlayerManager* manager);
  virtual ~MediaSourcePlayer();

  static bool IsTypeSupported(const std::vector<uint8>& scheme_uuid,
                              const std::string& security_level,
                              const std::string& container,
                              const std::vector<std::string>& codecs);

  // MediaPlayerAndroid implementation.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Pause() OVERRIDE;
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
  virtual void OnSeekRequestAck(unsigned seek_request_id) OVERRIDE;
  virtual void DemuxerReady(const DemuxerConfigs& configs) OVERRIDE;
  virtual void ReadFromDemuxerAck(const DemuxerData& data) OVERRIDE;
  virtual void DurationChanged(const base::TimeDelta& duration) OVERRIDE;
  virtual void SetDrmBridge(MediaDrmBridge* drm_bridge) OVERRIDE;

 private:
  // Update the current timestamp.
  void UpdateTimestamps(const base::TimeDelta& presentation_timestamp,
                        size_t audio_output_bytes);

  // Helper function for starting media playback.
  void StartInternal();

  // Playback is completed for one channel.
  void PlaybackCompleted(bool is_audio);

  // Called when the decoder finishes its task.
  void MediaDecoderCallback(
        bool is_audio, MediaCodecStatus status,
        const base::TimeDelta& presentation_timestamp,
        size_t audio_output_bytes);

  // Gets MediaCrypto object from |drm_bridge_|.
  base::android::ScopedJavaLocalRef<jobject> GetMediaCrypto();

  // Callback to notify that MediaCrypto is ready in |drm_bridge_|.
  void OnMediaCryptoReady();

  // Handle pending events when all the decoder jobs finished.
  void ProcessPendingEvents();

  // Helper method to configure the decoder jobs.
  void ConfigureVideoDecoderJob();
  void ConfigureAudioDecoderJob();

  // Flush the decoders and clean up all the data needs to be decoded.
  void ClearDecodingData();

  // Called to decoder more data.
  void DecodeMoreAudio();
  void DecodeMoreVideo();

  // Functions check whether audio/video is present.
  bool HasVideo();
  bool HasAudio();

  // Determine seekability based on duration.
  bool Seekable();

  // Called when the |decoder_starvation_callback_| times out.
  void OnDecoderStarved();

  // Starts the |decoder_starvation_callback_| task with the timeout value.
  void StartStarvationCallback(const base::TimeDelta& timeout);

  // Schedules a seek event in |pending_events_| and calls StopDecode() on all
  // the MediaDecoderJobs.
  void ScheduleSeekEventAndStopDecoding();

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

  // Pending event that the player needs to do.
  unsigned pending_event_;

  // ID to keep track of whether all the seek requests are acked.
  unsigned seek_request_id_;

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
  bool audio_finished_;
  bool video_finished_;
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

  // Decoder jobs.
  scoped_ptr<AudioDecoderJob, MediaDecoderJob::Deleter> audio_decoder_job_;
  scoped_ptr<VideoDecoderJob, MediaDecoderJob::Deleter> video_decoder_job_;

  bool reconfig_audio_decoder_;
  bool reconfig_video_decoder_;

  // A cancelable task that is posted when the audio decoder starts requesting
  // new data. This callback runs if no data arrives before the timeout period
  // elapses.
  base::CancelableClosure decoder_starvation_callback_;

  // Object to calculate the current audio timestamp for A/V sync.
  scoped_ptr<AudioTimestampHelper> audio_timestamp_helper_;

  // Weak pointer passed to media decoder jobs for callbacks.
  base::WeakPtrFactory<MediaSourcePlayer> weak_this_;

  MediaDrmBridge* drm_bridge_;

  friend class MediaSourcePlayerTest;
  DISALLOW_COPY_AND_ASSIGN(MediaSourcePlayer);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_SOURCE_PLAYER_H_
