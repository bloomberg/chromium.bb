// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See b/9357571.
const int kMediaCodecTimeoutInMilliseconds = 250;

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;

class DecoderThread : public base::Thread {
 public:
  virtual ~DecoderThread() {}
 protected:
  DecoderThread(const char* name) : base::Thread(name) { Start(); }
};

class AudioDecoderThread : public DecoderThread {
 public:
  AudioDecoderThread() : DecoderThread("MediaSource_AudioDecoderThread") {}
};

class VideoDecoderThread : public DecoderThread {
 public:
  VideoDecoderThread() : DecoderThread("MediaSource_VideoDecoderThread") {}
};

// TODO(qinmin): Check if it is tolerable to use worker pool to handle all the
// decoding tasks so that we don't need the global threads here.
// http://crbug.com/245750
base::LazyInstance<AudioDecoderThread>::Leaky
    g_audio_decoder_thread = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<VideoDecoderThread>::Leaky
    g_video_decoder_thread = LAZY_INSTANCE_INITIALIZER;

}

namespace media {

MediaDecoderJob::MediaDecoderJob(
    const scoped_refptr<base::MessageLoopProxy>& decoder_loop,
    MediaCodecBridge* media_codec_bridge,
    bool is_audio)
    : ui_loop_(base::MessageLoopProxy::current()),
      decoder_loop_(decoder_loop),
      media_codec_bridge_(media_codec_bridge),
      needs_flush_(false),
      is_audio_(is_audio),
      weak_this_(this),
      is_decoding_(false) {
}

MediaDecoderJob::~MediaDecoderJob() {}

// Class for managing audio decoding jobs.
class AudioDecoderJob : public MediaDecoderJob {
 public:
  virtual ~AudioDecoderJob() {}

  static AudioDecoderJob* Create(
      const AudioCodec audio_codec, int sample_rate, int channel_count,
      const uint8* extra_data, size_t extra_data_size, jobject media_crypto);

 private:
  AudioDecoderJob(MediaCodecBridge* media_codec_bridge);
};

// Class for managing video decoding jobs.
class VideoDecoderJob : public MediaDecoderJob {
 public:
  virtual ~VideoDecoderJob() {}

  static VideoDecoderJob* Create(
      const VideoCodec video_codec, const gfx::Size& size, jobject surface,
      jobject media_crypto);

 private:
  VideoDecoderJob(MediaCodecBridge* media_codec_bridge);
};

void MediaDecoderJob::Decode(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit& unit,
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback) {
  DCHECK(!is_decoding_);
  DCHECK(ui_loop_->BelongsToCurrentThread());
  is_decoding_ = true;
  decoder_loop_->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this), unit,
      start_time_ticks, start_presentation_timestamp, needs_flush_,
      callback));
  needs_flush_ = false;
}

MediaDecoderJob::DecodeStatus MediaDecoderJob::QueueInputBuffer(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit& unit) {
  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);
  int input_buf_index = media_codec_bridge_->DequeueInputBuffer(timeout);
  if (input_buf_index == MediaCodecBridge::INFO_MEDIA_CODEC_ERROR)
    return DECODE_FAILED;
  if (input_buf_index == MediaCodecBridge::INFO_TRY_AGAIN_LATER)
    return DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER;

  // TODO(qinmin): skip frames if video is falling far behind.
  DCHECK(input_buf_index >= 0);
  if (unit.end_of_stream || unit.data.empty()) {
    media_codec_bridge_->QueueEOS(input_buf_index);
  } else if (unit.key_id.empty()) {
    media_codec_bridge_->QueueInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
  } else {
    if (unit.iv.empty() || unit.subsamples.empty()) {
      LOG(ERROR) << "The access unit doesn't have iv or subsamples while it "
                 << "has key IDs!";
      return DECODE_FAILED;
    }
    media_codec_bridge_->QueueSecureInputBuffer(
        input_buf_index, &unit.data[0], unit.data.size(),
        reinterpret_cast<const uint8*>(&unit.key_id[0]), unit.key_id.size(),
        reinterpret_cast<const uint8*>(&unit.iv[0]), unit.iv.size(),
        &unit.subsamples[0], unit.subsamples.size(), unit.timestamp);
  }

  return DECODE_SUCCEEDED;
}

void MediaDecoderJob::DecodeInternal(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit& unit,
    const base::TimeTicks& start_time_ticks,
    const base::TimeDelta& start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  if (needs_flush) {
    DVLOG(1) << "DecodeInternal needs flush.";
    media_codec_bridge_->Reset();
  }

  DecodeStatus decode_status = QueueInputBuffer(unit);
  if (decode_status != DECODE_SUCCEEDED) {
    ui_loop_->PostTask(FROM_HERE,
        base::Bind(callback, decode_status, start_presentation_timestamp, 0));
    return;
  }

  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;
  bool end_of_stream = false;

  base::TimeDelta timeout = base::TimeDelta::FromMilliseconds(
      kMediaCodecTimeoutInMilliseconds);
  int outputBufferIndex = media_codec_bridge_->DequeueOutputBuffer(
      timeout, &offset, &size, &presentation_timestamp, &end_of_stream);

  if (end_of_stream)
    decode_status = DECODE_END_OF_STREAM;
  switch (outputBufferIndex) {
    case MediaCodecBridge::INFO_OUTPUT_BUFFERS_CHANGED:
      media_codec_bridge_->GetOutputBuffers();
      break;
    case MediaCodecBridge::INFO_OUTPUT_FORMAT_CHANGED:
      // TODO(qinmin): figure out what we should do if format changes.
      decode_status = DECODE_FORMAT_CHANGED;
      break;
    case MediaCodecBridge::INFO_TRY_AGAIN_LATER:
      decode_status = DECODE_TRY_DEQUEUE_OUTPUT_AGAIN_LATER;
      break;
    case MediaCodecBridge::INFO_MEDIA_CODEC_ERROR:
      decode_status = DECODE_FAILED;
      break;
    default:
      DCHECK_LE(0, outputBufferIndex);
      base::TimeDelta time_to_render;
      DCHECK(!start_time_ticks.is_null());
      if (!is_audio_) {
        time_to_render = presentation_timestamp - (base::TimeTicks::Now() -
            start_time_ticks + start_presentation_timestamp);
      }
      if (time_to_render >= base::TimeDelta()) {
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                       weak_this_.GetWeakPtr(), outputBufferIndex, size,
                       presentation_timestamp, callback, decode_status),
            time_to_render);
      } else {
        // TODO(qinmin): The codec is lagging behind, need to recalculate the
        // |start_presentation_timestamp_| and |start_time_ticks_|.
        DVLOG(1) << (is_audio_ ? "audio " : "video ")
            << "codec is lagging behind :" << time_to_render.InMicroseconds();
        ReleaseOutputBuffer(outputBufferIndex, size, presentation_timestamp,
                            callback, decode_status);
      }
      return;
  }
  ui_loop_->PostTask(FROM_HERE, base::Bind(
      callback, decode_status, start_presentation_timestamp, 0));
}

void MediaDecoderJob::ReleaseOutputBuffer(
    int outputBufferIndex, size_t size,
    const base::TimeDelta& presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback, DecodeStatus status) {
  // TODO(qinmin): Refactor this function. Maybe AudioDecoderJob should provide
  // its own ReleaseOutputBuffer().
  if (is_audio_) {
    static_cast<AudioCodecBridge*>(media_codec_bridge_.get())->PlayOutputBuffer(
        outputBufferIndex, size);
  }
  if (status != DECODE_END_OF_STREAM || size != 0u)
    media_codec_bridge_->ReleaseOutputBuffer(outputBufferIndex, !is_audio_);
  ui_loop_->PostTask(FROM_HERE, base::Bind(
      callback, status, presentation_timestamp, is_audio_ ? size : 0));
}

void MediaDecoderJob::OnDecodeCompleted() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  is_decoding_ = false;
}

void MediaDecoderJob::Flush() {
  // Do nothing, flush when the next Decode() happens.
  needs_flush_ = true;
}

void MediaDecoderJob::Release() {
  // If |decoding_| is false, there is nothing running on the decoder thread.
  // So it is safe to delete the MediaDecoderJob on the UI thread. However,
  // if we post a task to the decoder thread to delete object, then we cannot
  // immediately pass the surface to a new MediaDecoderJob instance because
  // the java surface is still owned by the old object. New decoder creation
  // will be blocked on the UI thread until the previous decoder gets deleted.
  // This introduces extra latency during config changes, and makes the logic in
  // MediaSourcePlayer more complicated.
  //
  // TODO(qinmin): Figure out the logic to passing the surface to a new
  // MediaDecoderJob instance after the previous one gets deleted on the decoder
  // thread.
  if (is_decoding_ && !decoder_loop_->BelongsToCurrentThread()) {
    DCHECK(ui_loop_->BelongsToCurrentThread());
    decoder_loop_->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

VideoDecoderJob* VideoDecoderJob::Create(
    const VideoCodec video_codec, const gfx::Size& size, jobject surface,
    jobject media_crypto) {
  scoped_ptr<VideoCodecBridge> codec(VideoCodecBridge::Create(video_codec));
  if (codec->Start(video_codec, size, surface, media_crypto))
    return new VideoDecoderJob(codec.release());
  return NULL;
}

VideoDecoderJob::VideoDecoderJob(MediaCodecBridge* media_codec_bridge)
    : MediaDecoderJob(g_video_decoder_thread.Pointer()->message_loop_proxy(),
                      media_codec_bridge,
                      false) {}

AudioDecoderJob* AudioDecoderJob::Create(
    const AudioCodec audio_codec,
    int sample_rate,
    int channel_count,
    const uint8* extra_data,
    size_t extra_data_size,
    jobject media_crypto) {
  scoped_ptr<AudioCodecBridge> codec(AudioCodecBridge::Create(audio_codec));
  if (codec->Start(audio_codec, sample_rate, channel_count, extra_data,
                   extra_data_size, true, media_crypto)) {
    return new AudioDecoderJob(codec.release());
  }
  return NULL;
}

AudioDecoderJob::AudioDecoderJob(MediaCodecBridge* media_codec_bridge)
    : MediaDecoderJob(g_audio_decoder_thread.Pointer()->message_loop_proxy(),
                      media_codec_bridge,
                      true) {}

MediaSourcePlayer::MediaSourcePlayer(
    int player_id,
    MediaPlayerManager* manager)
    : MediaPlayerAndroid(player_id, manager),
      pending_event_(NO_EVENT_PENDING),
      seek_request_id_(0),
      width_(0),
      height_(0),
      audio_codec_(kUnknownAudioCodec),
      video_codec_(kUnknownVideoCodec),
      num_channels_(0),
      sampling_rate_(0),
      audio_finished_(true),
      video_finished_(true),
      playing_(false),
      is_audio_encrypted_(false),
      is_video_encrypted_(false),
      clock_(&default_tick_clock_),
      reconfig_audio_decoder_(false),
      reconfig_video_decoder_(false),
      audio_access_unit_index_(0),
      video_access_unit_index_(0),
      waiting_for_audio_data_(false),
      waiting_for_video_data_(false),
      sync_decoder_jobs_(true),
      weak_this_(this),
      drm_bridge_(NULL) {
}

MediaSourcePlayer::~MediaSourcePlayer() {
  Release();
}

void MediaSourcePlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  // Ignore non-empty surface that is unprotected if |is_video_encrypted_| is
  // true.
  if (is_video_encrypted_ && !surface.IsEmpty() && !surface.is_protected())
    return;

  surface_ =  surface.Pass();
  pending_event_ |= SURFACE_CHANGE_EVENT_PENDING;
  if (pending_event_ & SEEK_EVENT_PENDING) {
    // Waiting for the seek to finish.
    return;
  }
  // Setting a new surface will require a new MediaCodec to be created.
  // Request a seek so that the new decoder will decode an I-frame first.
  // Or otherwise, the new MediaCodec might crash. See b/8950387.
  pending_event_ |= SEEK_EVENT_PENDING;
  ProcessPendingEvents();
}

bool MediaSourcePlayer::Seekable() {
  // If the duration TimeDelta, converted to milliseconds from microseconds,
  // is >= 2^31, then the media is assumed to be unbounded and unseekable.
  // 2^31 is the bound due to java player using 32-bit integer for time
  // values at millisecond resolution.
  return duration_ <
         base::TimeDelta::FromMilliseconds(std::numeric_limits<int32>::max());
}

void MediaSourcePlayer::Start() {
  playing_ = true;

  if (is_video_encrypted_)
    manager()->OnProtectedSurfaceRequested(player_id());

  StartInternal();
}

void MediaSourcePlayer::Pause() {
  // Since decoder jobs have their own thread, decoding is not fully paused
  // until all the decoder jobs call MediaDecoderCallback(). It is possible
  // that Start() is called while the player is waiting for
  // MediaDecoderCallback(). In that case, decoding will continue when
  // MediaDecoderCallback() is called.
  playing_ = false;
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::IsPlaying() {
  return playing_;
}

int MediaSourcePlayer::GetVideoWidth() {
  return width_;
}

int MediaSourcePlayer::GetVideoHeight() {
  return height_;
}

void MediaSourcePlayer::SeekTo(base::TimeDelta timestamp) {
  clock_.SetTime(timestamp, timestamp);
  if (audio_timestamp_helper_)
    audio_timestamp_helper_->SetBaseTimestamp(timestamp);
  pending_event_ |= SEEK_EVENT_PENDING;
  ProcessPendingEvents();
}

base::TimeDelta MediaSourcePlayer::GetCurrentTime() {
  return clock_.Elapsed();
}

base::TimeDelta MediaSourcePlayer::GetDuration() {
  return duration_;
}

void MediaSourcePlayer::Release() {
  ClearDecodingData();
  audio_decoder_job_.reset();
  video_decoder_job_.reset();
  reconfig_audio_decoder_ = false;
  reconfig_video_decoder_ = false;
  playing_ = false;
  pending_event_ = NO_EVENT_PENDING;
  surface_ = gfx::ScopedJavaSurface();
  ReleaseMediaResourcesFromManager();
}

void MediaSourcePlayer::SetVolume(float leftVolume, float rightVolume) {
}

bool MediaSourcePlayer::CanPause() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekForward() {
  return Seekable();
}

bool MediaSourcePlayer::CanSeekBackward() {
  return Seekable();
}

bool MediaSourcePlayer::IsPlayerReady() {
  return audio_decoder_job_ || video_decoder_job_;
}

void MediaSourcePlayer::StartInternal() {
  // If there are pending events, wait for them finish.
  if (pending_event_ != NO_EVENT_PENDING)
    return;

  // Create decoder jobs if they are not created
  ConfigureAudioDecoderJob();
  ConfigureVideoDecoderJob();


  // If one of the decoder job is not ready, do nothing.
  if ((HasAudio() && !audio_decoder_job_) ||
      (HasVideo() && !video_decoder_job_)) {
    return;
  }

  audio_finished_ = false;
  video_finished_ = false;
  sync_decoder_jobs_ = true;
  SyncAndStartDecoderJobs();
}

void MediaSourcePlayer::DemuxerReady(
    const MediaPlayerHostMsg_DemuxerReady_Params& params) {
  duration_ = base::TimeDelta::FromMilliseconds(params.duration_ms);
  clock_.SetDuration(duration_);

  audio_codec_ = params.audio_codec;
  num_channels_ = params.audio_channels;
  sampling_rate_ = params.audio_sampling_rate;
  is_audio_encrypted_ = params.is_audio_encrypted;
  audio_extra_data_ = params.audio_extra_data;
  if (HasAudio()) {
    DCHECK_GT(num_channels_, 0);
    audio_timestamp_helper_.reset(new AudioTimestampHelper(sampling_rate_));
    audio_timestamp_helper_->SetBaseTimestamp(GetCurrentTime());
  } else {
    audio_timestamp_helper_.reset();
  }

  video_codec_ = params.video_codec;
  width_ = params.video_size.width();
  height_ = params.video_size.height();
  is_video_encrypted_ = params.is_video_encrypted;

  OnMediaMetadataChanged(duration_, width_, height_, true);

  if (pending_event_ & CONFIG_CHANGE_EVENT_PENDING) {
    if (reconfig_audio_decoder_)
      ConfigureAudioDecoderJob();

    // If there is a pending surface change, we can merge it with the config
    // change.
    if (reconfig_video_decoder_) {
      pending_event_ &= ~SURFACE_CHANGE_EVENT_PENDING;
      ConfigureVideoDecoderJob();
    }
    pending_event_ &= ~CONFIG_CHANGE_EVENT_PENDING;
    if (playing_)
      StartInternal();
  }
}

void MediaSourcePlayer::ReadFromDemuxerAck(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
  DCHECK_LT(0u, params.access_units.size());
  if (params.type == DemuxerStream::AUDIO)
    waiting_for_audio_data_ = false;
  else
    waiting_for_video_data_ = false;

  // If there is a pending seek request, ignore the data from the chunk demuxer.
  // The data will be requested later when OnSeekRequestAck() is called.
  if (pending_event_ & SEEK_EVENT_PENDING)
    return;

  if (params.type == DemuxerStream::AUDIO) {
    DCHECK_EQ(0u, audio_access_unit_index_);
    received_audio_ = params;
  } else {
    DCHECK_EQ(0u, video_access_unit_index_);
    received_video_ = params;
  }

  if (pending_event_ != NO_EVENT_PENDING || !playing_)
    return;

  if (sync_decoder_jobs_) {
    SyncAndStartDecoderJobs();
    return;
  }

  if (params.type == DemuxerStream::AUDIO)
    DecodeMoreAudio();
  else
    DecodeMoreVideo();
}

void MediaSourcePlayer::DurationChanged(const base::TimeDelta& duration) {
  duration_ = duration;
  clock_.SetDuration(duration_);
}

void MediaSourcePlayer::SetDrmBridge(MediaDrmBridge* drm_bridge) {
  // Currently we don't support DRM change during the middle of playback, even
  // if the player is paused.
  // TODO(qinmin): support DRM change after playback has started.
  // http://crbug.com/253792.
  if (GetCurrentTime() > base::TimeDelta()) {
    LOG(INFO) << "Setting DRM bridge after play back has started. "
              << "This is not well supported!";
  }

  drm_bridge_ = drm_bridge;

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::OnSeekRequestAck(unsigned seek_request_id) {
  DVLOG(1) << "OnSeekRequestAck(" << seek_request_id << ")";
  // Do nothing until the most recent seek request is processed.
  if (seek_request_id_ != seek_request_id)
    return;
  pending_event_ &= ~SEEK_EVENT_PENDING;
  OnSeekComplete();
  ProcessPendingEvents();
}

void MediaSourcePlayer::UpdateTimestamps(
    const base::TimeDelta& presentation_timestamp, size_t audio_output_bytes) {
  if (audio_output_bytes > 0) {
    audio_timestamp_helper_->AddFrames(
        audio_output_bytes / (kBytesPerAudioOutputSample * num_channels_));
    clock_.SetMaxTime(audio_timestamp_helper_->GetTimestamp());
  } else {
    clock_.SetMaxTime(presentation_timestamp);
  }

  OnTimeUpdated();
}

void MediaSourcePlayer::ProcessPendingEvents() {
  // Wait for all the decoding jobs to finish before processing pending tasks.
  if ((audio_decoder_job_ && audio_decoder_job_->is_decoding()) ||
      (video_decoder_job_ && video_decoder_job_->is_decoding())) {
    return;
  }

  if (pending_event_ & SEEK_EVENT_PENDING) {
    ClearDecodingData();
    manager()->OnMediaSeekRequest(
        player_id(), GetCurrentTime(), ++seek_request_id_);
    return;
  }

  start_time_ticks_ = base::TimeTicks();
  if (pending_event_ & CONFIG_CHANGE_EVENT_PENDING) {
    DCHECK(reconfig_audio_decoder_ || reconfig_video_decoder_);
    manager()->OnMediaConfigRequest(player_id());
    return;
  }

  if (pending_event_ & SURFACE_CHANGE_EVENT_PENDING) {
    video_decoder_job_.reset();
    ConfigureVideoDecoderJob();
    pending_event_ &= ~SURFACE_CHANGE_EVENT_PENDING;
  }

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::MediaDecoderCallback(
    bool is_audio, MediaDecoderJob::DecodeStatus decode_status,
    const base::TimeDelta& presentation_timestamp, size_t audio_output_bytes) {
  if (is_audio && audio_decoder_job_)
    audio_decoder_job_->OnDecodeCompleted();
  if (!is_audio && video_decoder_job_)
    video_decoder_job_->OnDecodeCompleted();

  if (is_audio)
    decoder_starvation_callback_.Cancel();

  if (decode_status == MediaDecoderJob::DECODE_FAILED) {
    Release();
    OnMediaError(MEDIA_ERROR_DECODE);
    return;
  }

  if (decode_status != MediaDecoderJob::DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER) {
    if (is_audio)
      audio_access_unit_index_++;
    else
      video_access_unit_index_++;
  }

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (decode_status == MediaDecoderJob::DECODE_SUCCEEDED &&
      (is_audio || !HasAudio())) {
    UpdateTimestamps(presentation_timestamp, audio_output_bytes);
  }

  if (decode_status == MediaDecoderJob::DECODE_END_OF_STREAM) {
    PlaybackCompleted(is_audio);
    return;
  }

  if (!playing_) {
    if (is_audio || !HasAudio())
      clock_.Pause();
    return;
  }

  if (sync_decoder_jobs_) {
    SyncAndStartDecoderJobs();
    return;
  }

  base::TimeDelta current_timestamp = GetCurrentTime();
  if (is_audio) {
    if (decode_status == MediaDecoderJob::DECODE_SUCCEEDED) {
      base::TimeDelta timeout =
          audio_timestamp_helper_->GetTimestamp() - current_timestamp;
      StartStarvationCallback(timeout);
    }
    if (!HasAudioData())
      RequestAudioData();
    else
      DecodeMoreAudio();
    return;
  }

  if (!HasAudio() && decode_status == MediaDecoderJob::DECODE_SUCCEEDED) {
    DCHECK(current_timestamp <= presentation_timestamp);
    // For video only streams, fps can be estimated from the difference
    // between the previous and current presentation timestamps. The
    // previous presentation timestamp is equal to current_timestamp.
    // TODO(qinmin): determine whether 2 is a good coefficient for estimating
    // video frame timeout.
    StartStarvationCallback(2 * (presentation_timestamp - current_timestamp));
  }
  if (!HasVideoData())
    RequestVideoData();
  else
    DecodeMoreVideo();
}

void MediaSourcePlayer::DecodeMoreAudio() {
  DCHECK(!audio_decoder_job_->is_decoding());
  DCHECK(HasAudioData());

  if (DemuxerStream::kConfigChanged ==
      received_audio_.access_units[audio_access_unit_index_].status) {
    // Wait for demuxer ready message.
    reconfig_audio_decoder_ = true;
    pending_event_ |= CONFIG_CHANGE_EVENT_PENDING;
    received_audio_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
    audio_access_unit_index_ = 0;
    ProcessPendingEvents();
    return;
  }

  audio_decoder_job_->Decode(
      received_audio_.access_units[audio_access_unit_index_],
      start_time_ticks_, start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                 weak_this_.GetWeakPtr(), true));
}

void MediaSourcePlayer::DecodeMoreVideo() {
  DVLOG(1) << "DecodeMoreVideo()";
  DCHECK(!video_decoder_job_->is_decoding());
  DCHECK(HasVideoData());

  if (DemuxerStream::kConfigChanged ==
      received_video_.access_units[video_access_unit_index_].status) {
    // Wait for demuxer ready message.
    reconfig_video_decoder_ = true;
    pending_event_ |= CONFIG_CHANGE_EVENT_PENDING;
    received_video_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
    video_access_unit_index_ = 0;
    ProcessPendingEvents();
    return;
  }

  DVLOG(3) << "VideoDecoderJob::Decode(" << video_access_unit_index_ << ", "
           << start_time_ticks_.ToInternalValue() << ", "
           << start_presentation_timestamp_.InMilliseconds() << ")";
  video_decoder_job_->Decode(
      received_video_.access_units[video_access_unit_index_],
      start_time_ticks_, start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                 weak_this_.GetWeakPtr(), false));
}

void MediaSourcePlayer::PlaybackCompleted(bool is_audio) {
  if (is_audio)
    audio_finished_ = true;
  else
    video_finished_ = true;

  if ((!HasAudio() || audio_finished_) && (!HasVideo() || video_finished_)) {
    playing_ = false;
    clock_.Pause();
    start_time_ticks_ = base::TimeTicks();
    OnPlaybackComplete();
  }
}

void MediaSourcePlayer::ClearDecodingData() {
  DVLOG(1) << "ClearDecodingData()";
  if (audio_decoder_job_)
    audio_decoder_job_->Flush();
  if (video_decoder_job_)
    video_decoder_job_->Flush();
  start_time_ticks_ = base::TimeTicks();
  received_audio_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  received_video_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  audio_access_unit_index_ = 0;
  video_access_unit_index_ = 0;
  waiting_for_audio_data_ = false;
  waiting_for_video_data_ = false;
}

bool MediaSourcePlayer::HasVideo() {
  return kUnknownVideoCodec != video_codec_;
}

bool MediaSourcePlayer::HasAudio() {
  return kUnknownAudioCodec != audio_codec_;
}

void MediaSourcePlayer::ConfigureAudioDecoderJob() {
  if (!HasAudio()) {
    audio_decoder_job_.reset();
    return;
  }

  // Create audio decoder job only if config changes.
  if (audio_decoder_job_ && !reconfig_audio_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_codec;
  if (is_audio_encrypted_) {
    if (drm_bridge_) {
      media_codec = drm_bridge_->GetMediaCrypto();
      // TODO(qinmin): currently we assume MediaCrypto is available whenever
      // MediaDrmBridge is constructed. This will change if we want to support
      // more general uses cases of EME.
      DCHECK(!media_codec.is_null());
    } else {
      // Don't create the decoder job if |drm_bridge_| is not set,
      // so StartInternal() will not proceed.
      LOG(INFO) << "MediaDrmBridge is not available when creating decoder "
                << "for encrypted audio stream.";
      return;
    }
  }

  audio_decoder_job_.reset(AudioDecoderJob::Create(
      audio_codec_, sampling_rate_, num_channels_, &audio_extra_data_[0],
      audio_extra_data_.size(), media_codec.obj()));

  if (audio_decoder_job_)
    reconfig_audio_decoder_ =  false;
}

void MediaSourcePlayer::ConfigureVideoDecoderJob() {
  if (!HasVideo() || surface_.IsEmpty()) {
    video_decoder_job_.reset();
    return;
  }

  // Create video decoder job only if config changes.
  if (video_decoder_job_ && !reconfig_video_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_codec;
  if (is_video_encrypted_) {
    if (drm_bridge_) {
      media_codec = drm_bridge_->GetMediaCrypto();
      DCHECK(!media_codec.is_null());
    } else {
      LOG(INFO) << "MediaDrmBridge is not available when creating decoder "
                << "for encrypted video stream.";
      return;
    }
  }

  // Release the old VideoDecoderJob first so the surface can get released.
  // Android does not allow 2 MediaCodec instances use the same surface.
  video_decoder_job_.reset();
  // Create the new VideoDecoderJob.
  video_decoder_job_.reset(VideoDecoderJob::Create(
      video_codec_, gfx::Size(width_, height_), surface_.j_surface().obj(),
      media_codec.obj()));
  if (video_decoder_job_)
    reconfig_video_decoder_ = false;

  // Inform the fullscreen view the player is ready.
  // TODO(qinmin): refactor MediaPlayerBridge so that we have a better way
  // to inform ContentVideoView.
  OnMediaMetadataChanged(duration_, width_, height_, true);
}

void MediaSourcePlayer::OnDecoderStarved() {
  sync_decoder_jobs_ = true;
}

void MediaSourcePlayer::StartStarvationCallback(
    const base::TimeDelta& timeout) {
  decoder_starvation_callback_.Reset(
      base::Bind(&MediaSourcePlayer::OnDecoderStarved,
                 weak_this_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, decoder_starvation_callback_.callback(), timeout);
}

void MediaSourcePlayer::SyncAndStartDecoderJobs() {
  // For streams with both audio and video, send the request for video too.
  // However, don't wait for the response so that we won't have lots of
  // noticeable pauses in the audio. Video will sync with audio by itself.
  if (HasVideo() && !HasVideoData()) {
    RequestVideoData();
    if (!HasAudio())
      return;
  }
  if (HasAudio() && !HasAudioData()) {
    RequestAudioData();
    return;
  }
  start_time_ticks_ = base::TimeTicks::Now();
  start_presentation_timestamp_ = GetCurrentTime();
  if (!clock_.IsPlaying())
    clock_.Play();
  if (HasAudioData() && !audio_decoder_job_->is_decoding())
    DecodeMoreAudio();
  if (HasVideoData() && !video_decoder_job_->is_decoding())
    DecodeMoreVideo();
  sync_decoder_jobs_ = false;
}

void MediaSourcePlayer::RequestAudioData() {
  DVLOG(2) << "RequestAudioData()";
  DCHECK(HasAudio());

  if (waiting_for_audio_data_)
    return;

  manager()->OnReadFromDemuxer(player_id(), DemuxerStream::AUDIO);
  received_audio_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  audio_access_unit_index_ = 0;
  waiting_for_audio_data_ = true;
}

void MediaSourcePlayer::RequestVideoData() {
  DVLOG(2) << "RequestVideoData()";
  DCHECK(HasVideo());
  if (waiting_for_video_data_)
    return;

  manager()->OnReadFromDemuxer(player_id(), DemuxerStream::VIDEO);
  received_video_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  video_access_unit_index_ = 0;
  waiting_for_video_data_ = true;
}

bool MediaSourcePlayer::HasAudioData() const {
  return audio_access_unit_index_ < received_audio_.access_units.size();
}

bool MediaSourcePlayer::HasVideoData() const {
  return video_access_unit_index_ < received_video_.access_units.size();
}

}  // namespace media
