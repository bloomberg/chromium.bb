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
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_player_android.h"
#include "media/base/clock.h"
#include "media/base/media_export.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioDecoderJob;
class AudioTimestampHelper;
class VideoDecoderJob;

// Class for managing all the decoding tasks. Each decoding task will be posted
// onto the same thread. The thread will be stopped once Stop() is called.
class MediaDecoderJob {
 public:
  enum DecodeStatus {
    DECODE_SUCCEEDED,
    DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER,
    DECODE_TRY_DEQUEUE_OUTPUT_AGAIN_LATER,
    DECODE_FORMAT_CHANGED,
    DECODE_INPUT_END_OF_STREAM,
    DECODE_OUTPUT_END_OF_STREAM,
    DECODE_FAILED,
  };

  virtual ~MediaDecoderJob();

  // Callback when a decoder job finishes its work. Args: whether decode
  // finished successfully, presentation time, audio output bytes.
  typedef base::Callback<void(DecodeStatus, const base::TimeDelta&,
                              size_t)> DecoderCallback;

  // Called by MediaSourcePlayer to decode some data.
  void Decode(const AccessUnit& unit,
              const base::TimeTicks& start_time_ticks,
              const base::TimeDelta& start_presentation_timestamp,
              const MediaDecoderJob::DecoderCallback& callback);

  // Flush the decoder.
  void Flush();

  // Causes this instance to be deleted on the thread it is bound to.
  void Release();

  // Called on the UI thread to indicate that one decode cycle has completed.
  void OnDecodeCompleted();

  bool is_decoding() const { return is_decoding_; }

 protected:
  MediaDecoderJob(const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
                  MediaCodecBridge* media_codec_bridge,
                  bool is_audio);

  // Release the output buffer and render it.
  void ReleaseOutputBuffer(
      int outputBufferIndex, size_t size,
      const base::TimeDelta& presentation_timestamp,
      const MediaDecoderJob::DecoderCallback& callback, DecodeStatus status);

  DecodeStatus QueueInputBuffer(const AccessUnit& unit);

  // Helper function to decoder data on |thread_|. |unit| contains all the data
  // to be decoded. |start_time_ticks| and |start_presentation_timestamp|
  // represent the system time and the presentation timestamp when the first
  // frame is rendered. We use these information to estimate when the current
  // frame should be rendered. If |needs_flush| is true, codec needs to be
  // flushed at the beginning of this call.
  void DecodeInternal(const AccessUnit& unit,
                      const base::TimeTicks& start_time_ticks,
                      const base::TimeDelta& start_presentation_timestamp,
                      bool needs_flush,
                      const MediaDecoderJob::DecoderCallback& callback);

  // The UI message loop where callbacks should be dispatched.
  scoped_refptr<base::MessageLoopProxy> ui_loop_;

  // The message loop that decoder job runs on.
  scoped_refptr<base::MessageLoopProxy> decoder_loop_;

  // The media codec bridge used for decoding.
  scoped_ptr<MediaCodecBridge> media_codec_bridge_;

  // Whether the decoder needs to be flushed.
  bool needs_flush_;

  // Whether this is an audio decoder.
  bool is_audio_;

  // Whether input EOS is encountered.
  bool input_eos_encountered_;

  // Weak pointer passed to media decoder jobs for callbacks. It is bounded to
  // the decoder thread.
  base::WeakPtrFactory<MediaDecoderJob> weak_this_;

  // Whether the decoder is actively decoding data.
  bool is_decoding_;
};

struct DecoderJobDeleter {
  inline void operator()(MediaDecoderJob* ptr) const { ptr->Release(); }
};

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
  virtual void DemuxerReady(
      const MediaPlayerHostMsg_DemuxerReady_Params& params) OVERRIDE;
  virtual void ReadFromDemuxerAck(
      const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) OVERRIDE;
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
        bool is_audio, MediaDecoderJob::DecodeStatus decode_status,
        const base::TimeDelta& presentation_timestamp,
        size_t audio_output_bytes);

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

  // Called to sync decoder jobs. This call requests data from chunk demuxer
  // first. Then it updates |start_time_ticks_| and
  // |start_presentation_timestamp_| so that video can resync with audio.
  void SyncAndStartDecoderJobs();

  // Functions that send IPC requests to the renderer process for more
  // audio/video data. Returns true if a request has been sent and the decoder
  // needs to wait, or false otherwise.
  void RequestAudioData();
  void RequestVideoData();

  // Check whether audio or video data is available for decoders to consume.
  bool HasAudioData() const;
  bool HasVideoData() const;

  // Helper function to set the volume.
  void SetVolumeInternal();

  enum PendingEventFlags {
    NO_EVENT_PENDING = 0,
    SEEK_EVENT_PENDING = 1 << 0,
    SURFACE_CHANGE_EVENT_PENDING = 1 << 1,
    CONFIG_CHANGE_EVENT_PENDING = 1 << 2,
  };
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

  // Decoder jobs
  scoped_ptr<AudioDecoderJob, DecoderJobDeleter> audio_decoder_job_;
  scoped_ptr<VideoDecoderJob, DecoderJobDeleter> video_decoder_job_;

  bool reconfig_audio_decoder_;
  bool reconfig_video_decoder_;

  // These variables keep track of the current decoding data.
  // TODO(qinmin): remove these variables when we no longer relies on IPC for
  // data passing.
  size_t audio_access_unit_index_;
  size_t video_access_unit_index_;
  bool waiting_for_audio_data_;
  bool waiting_for_video_data_;
  MediaPlayerHostMsg_ReadFromDemuxerAck_Params received_audio_;
  MediaPlayerHostMsg_ReadFromDemuxerAck_Params received_video_;

  // A cancelable task that is posted when the audio decoder starts requesting
  // new data. This callback runs if no data arrives before the timeout period
  // elapses.
  base::CancelableClosure decoder_starvation_callback_;

  // Whether the audio and video decoder jobs should resync with each other.
  bool sync_decoder_jobs_;

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
