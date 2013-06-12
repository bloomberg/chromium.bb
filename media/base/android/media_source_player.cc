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
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "media/base/android/media_codec_bridge.h"
#include "media/base/android/media_player_manager.h"

namespace {

// Timeout value for media codec operations. Because the first
// DequeInputBuffer() can take about 150 milliseconds, use 250 milliseconds
// here. See b/9357571.
const int kMediaCodecTimeoutInMicroseconds = 250000;

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

MediaDecoderJob::MediaDecoderJob(base::Thread* thread, bool is_audio)
    : message_loop_(base::MessageLoopProxy::current()),
      thread_(thread),
      needs_flush_(false),
      is_audio_(is_audio),
      weak_this_(this),
      decoding_(false) {
}

MediaDecoderJob::~MediaDecoderJob() {}

// Class for managing audio decoding jobs.
class AudioDecoderJob : public MediaDecoderJob {
 public:
  AudioDecoderJob(
      const AudioCodec audio_codec, int sample_rate,
      int channel_count, const uint8* extra_data, size_t extra_data_size);
  virtual ~AudioDecoderJob() {}
};

// Class for managing video decoding jobs.
class VideoDecoderJob : public MediaDecoderJob {
 public:
  VideoDecoderJob(
      const VideoCodec video_codec, const gfx::Size& size, jobject surface);
  virtual ~VideoDecoderJob() {}

  void Configure(
      const VideoCodec codec, const gfx::Size& size, jobject surface);
};

void MediaDecoderJob::Decode(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit& unit,
    const base::Time& start_wallclock_time,
    const base::TimeDelta& start_presentation_timestamp,
    const MediaDecoderJob::DecoderCallback& callback) {
  DCHECK(!decoding_);
  decoding_ = true;
  thread_->message_loop()->PostTask(FROM_HERE, base::Bind(
      &MediaDecoderJob::DecodeInternal, base::Unretained(this), unit,
      start_wallclock_time, start_presentation_timestamp, needs_flush_,
      callback));
  needs_flush_ = false;
}

void MediaDecoderJob::DecodeInternal(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params::AccessUnit& unit,
    const base::Time& start_wallclock_time,
    const base::TimeDelta& start_presentation_timestamp,
    bool needs_flush,
    const MediaDecoderJob::DecoderCallback& callback) {
  if (needs_flush)
    media_codec_bridge_->Reset();
  base::TimeDelta timeout = base::TimeDelta::FromMicroseconds(
      kMediaCodecTimeoutInMicroseconds);
  int input_buf_index = media_codec_bridge_->DequeueInputBuffer(timeout);
  if (input_buf_index == MediaCodecBridge::INFO_MEDIA_CODEC_ERROR) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        callback, DECODE_FAILED, start_presentation_timestamp,
        start_wallclock_time, false));
    return;
  }
  // TODO(qinmin): skip frames if video is falling far behind.
  if (input_buf_index >= 0) {
    if (unit.end_of_stream) {
      media_codec_bridge_->QueueEOS(input_buf_index);
    } else {
      media_codec_bridge_->QueueInputBuffer(
          input_buf_index, &unit.data[0], unit.data.size(), unit.timestamp);
    }
  }

  size_t offset = 0;
  size_t size = 0;
  base::TimeDelta presentation_timestamp;
  bool end_of_stream = false;
  DecodeStatus decode_status = DECODE_SUCCEEDED;

  int outputBufferIndex = media_codec_bridge_->DequeueOutputBuffer(
      timeout, &offset, &size, &presentation_timestamp, &end_of_stream);
  switch (outputBufferIndex) {
    case MediaCodecBridge::INFO_OUTPUT_BUFFERS_CHANGED:
      media_codec_bridge_->GetOutputBuffers();
      break;
    case MediaCodecBridge::INFO_OUTPUT_FORMAT_CHANGED:
      // TODO(qinmin): figure out what we should do if format changes.
      break;
    case MediaCodecBridge::INFO_TRY_AGAIN_LATER:
      decode_status = DECODE_TRY_AGAIN_LATER;
      break;
    case MediaCodecBridge::INFO_MEDIA_CODEC_ERROR:
      decode_status = DECODE_FAILED;
      break;
    default:
      DCHECK_LE(0, outputBufferIndex);
      if (size == 0 && end_of_stream)
        break;
      base::TimeDelta time_to_render;
      if (!start_wallclock_time.is_null()) {
        time_to_render = presentation_timestamp - (base::Time::Now() -
            start_wallclock_time + start_presentation_timestamp);
      }
      if (time_to_render >= base::TimeDelta()) {
        base::MessageLoop::current()->PostDelayedTask(
            FROM_HERE,
            base::Bind(&MediaDecoderJob::ReleaseOutputBuffer,
                       weak_this_.GetWeakPtr(), outputBufferIndex, size,
                       presentation_timestamp, end_of_stream, callback),
            time_to_render);
      } else {
        // TODO(qinmin): The codec is lagging behind, need to recalculate the
        // |start_presentation_timestamp_| and |start_wallclock_time_|.
        DVLOG(1) << (is_audio_ ? "audio " : "video ")
            << "codec is lagging behind :" << time_to_render.InMicroseconds();
        ReleaseOutputBuffer(outputBufferIndex, size, presentation_timestamp,
                            end_of_stream, callback);
      }
      return;
  }
  message_loop_->PostTask(FROM_HERE, base::Bind(
      callback, decode_status, start_presentation_timestamp,
      start_wallclock_time, end_of_stream));
}

void MediaDecoderJob::ReleaseOutputBuffer(
    int outputBufferIndex, size_t size,
    const base::TimeDelta& presentation_timestamp,
    bool end_of_stream, const MediaDecoderJob::DecoderCallback& callback) {
  // TODO(qinmin): Refactor this function. Maybe AudioDecoderJob should provide
  // its own ReleaseOutputBuffer().
  if (is_audio_) {
    static_cast<AudioCodecBridge*>(media_codec_bridge_.get())->PlayOutputBuffer(
        outputBufferIndex, size);
  }
  media_codec_bridge_->ReleaseOutputBuffer(outputBufferIndex, !is_audio_);
  message_loop_->PostTask(FROM_HERE, base::Bind(
      callback, DECODE_SUCCEEDED, presentation_timestamp, base::Time::Now(),
      end_of_stream));
}

void MediaDecoderJob::OnDecodeCompleted() {
  decoding_ = false;
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
  if (decoding_ && thread_->message_loop() != base::MessageLoop::current())
    thread_->message_loop()->DeleteSoon(FROM_HERE, this);
  else
    delete this;
}

VideoDecoderJob::VideoDecoderJob(
    const VideoCodec video_codec, const gfx::Size& size, jobject surface)
    : MediaDecoderJob(g_video_decoder_thread.Pointer(), false) {
  scoped_ptr<VideoCodecBridge> codec(VideoCodecBridge::Create(video_codec));
  codec->Start(video_codec, size, surface);
  media_codec_bridge_.reset(codec.release());
}

AudioDecoderJob::AudioDecoderJob(
    const AudioCodec audio_codec,
    int sample_rate,
    int channel_count,
    const uint8* extra_data,
    size_t extra_data_size)
    : MediaDecoderJob(g_audio_decoder_thread.Pointer(), true) {
  scoped_ptr<AudioCodecBridge> codec(AudioCodecBridge::Create(audio_codec));
  codec->Start(audio_codec, sample_rate, channel_count, extra_data,
               extra_data_size, true);
  media_codec_bridge_.reset(codec.release());
}

MediaSourcePlayer::MediaSourcePlayer(
    int player_id,
    MediaPlayerManager* manager)
    : MediaPlayerAndroid(player_id, manager),
      pending_event_(NO_EVENT_PENDING),
      active_decoding_tasks_(0),
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
      reconfig_audio_decoder_(false),
      reconfig_video_decoder_(false),
      audio_access_unit_index_(0),
      video_access_unit_index_(0),
      waiting_for_audio_data_(false),
      waiting_for_video_data_(false),
      weak_this_(this) {
}

MediaSourcePlayer::~MediaSourcePlayer() {
  Release();
}

void MediaSourcePlayer::SetVideoSurface(gfx::ScopedJavaSurface surface) {
  surface_ =  surface.Pass();
  pending_event_ |= SURFACE_CHANGE_EVENT_PENDING;
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

  CreateAudioDecoderJob();
  CreateVideoDecoderJob();

  StartInternal();
}

void MediaSourcePlayer::Pause() {
  playing_ = false;
  start_wallclock_time_ = base::Time();
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
  last_presentation_timestamp_ = timestamp;
  pending_event_ |= SEEK_EVENT_PENDING;
  ProcessPendingEvents();
}

base::TimeDelta MediaSourcePlayer::GetCurrentTime() {
  return last_presentation_timestamp_;
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
  active_decoding_tasks_ = 0;
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
  // Do nothing if the decoders are already running.
  if (active_decoding_tasks_ > 0 || pending_event_ != NO_EVENT_PENDING)
    return;

  // If one of the decoder job is not ready, do nothing.
  if ((HasAudio() && !audio_decoder_job_) ||
      (HasVideo() && !video_decoder_job_)) {
    return;
  }

  if (HasAudio()) {
    audio_finished_ = false;
    DecodeMoreAudio();
  }
  if (HasVideo()) {
    video_finished_ = false;
    DecodeMoreVideo();
  }
}

void MediaSourcePlayer::DemuxerReady(
    const MediaPlayerHostMsg_DemuxerReady_Params& params) {
  duration_ = base::TimeDelta::FromMilliseconds(params.duration_ms);
  width_ = params.video_size.width();
  height_ = params.video_size.height();
  num_channels_ = params.audio_channels;
  sampling_rate_ = params.audio_sampling_rate;
  audio_codec_ = params.audio_codec;
  video_codec_ = params.video_codec;
  audio_extra_data_ = params.audio_extra_data;
  OnMediaMetadataChanged(duration_, width_, height_, true);
  if (pending_event_ & CONFIG_CHANGE_EVENT_PENDING) {
    if (reconfig_audio_decoder_) {
      CreateAudioDecoderJob();
    }
    // If there is a pending surface change, we can merge it with the config
    // change.
    if (reconfig_video_decoder_) {
      pending_event_ &= ~SURFACE_CHANGE_EVENT_PENDING;
      CreateVideoDecoderJob();
    }
    pending_event_ &= ~CONFIG_CHANGE_EVENT_PENDING;
    if (playing_ && pending_event_ == NO_EVENT_PENDING)
      StartInternal();
  }
}

void MediaSourcePlayer::ReadFromDemuxerAck(
    const MediaPlayerHostMsg_ReadFromDemuxerAck_Params& params) {
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
    if (!pending_event_)
      DecodeMoreAudio();
  } else {
    DCHECK_EQ(0u, video_access_unit_index_);
    received_video_ = params;
    if (!pending_event_)
      DecodeMoreVideo();
  }
}

void MediaSourcePlayer::DurationChanged(const base::TimeDelta& duration) {
  duration_ = duration;
}

void MediaSourcePlayer::OnSeekRequestAck(unsigned seek_request_id) {
  // Do nothing until the most recent seek request is processed.
  if (seek_request_id_ != seek_request_id)
    return;
  pending_event_ &= ~SEEK_EVENT_PENDING;
  OnSeekComplete();
  ProcessPendingEvents();
}

void MediaSourcePlayer::UpdateTimestamps(
    const base::TimeDelta& presentation_timestamp,
    const base::Time& wallclock_time) {
  last_presentation_timestamp_ = presentation_timestamp;
  OnTimeUpdated();
  if (start_wallclock_time_.is_null() && playing_) {
    start_wallclock_time_ = wallclock_time;
    start_presentation_timestamp_ = last_presentation_timestamp_;
  }
}

void MediaSourcePlayer::ProcessPendingEvents() {
  // Wait for all the decoding jobs to finish before processing pending tasks.
  if (active_decoding_tasks_ > 0)
    return;

  if (pending_event_ & SEEK_EVENT_PENDING) {
    ClearDecodingData();
    manager()->OnMediaSeekRequest(
        player_id(), last_presentation_timestamp_, ++seek_request_id_);
    return;
  }

  start_wallclock_time_ = base::Time();
  if (pending_event_ & CONFIG_CHANGE_EVENT_PENDING) {
    DCHECK(reconfig_audio_decoder_ || reconfig_video_decoder_);
    manager()->OnMediaConfigRequest(player_id());
    return;
  }

  if (pending_event_ & SURFACE_CHANGE_EVENT_PENDING) {
    video_decoder_job_.reset();
    CreateVideoDecoderJob();
    pending_event_ &= ~SURFACE_CHANGE_EVENT_PENDING;
  }

  if (playing_ && pending_event_ == NO_EVENT_PENDING)
    StartInternal();
}

void MediaSourcePlayer::MediaDecoderCallback(
    bool is_audio, MediaDecoderJob::DecodeStatus decode_status,
    const base::TimeDelta& presentation_timestamp,
    const base::Time& wallclock_time, bool end_of_stream) {
  if (active_decoding_tasks_ > 0)
    active_decoding_tasks_--;

  if (is_audio && audio_decoder_job_)
    audio_decoder_job_->OnDecodeCompleted();
  if (!is_audio && video_decoder_job_)
    video_decoder_job_->OnDecodeCompleted();

  if (decode_status == MediaDecoderJob::DECODE_FAILED) {
    Release();
    OnMediaError(MEDIA_ERROR_DECODE);
    return;
  } else if (decode_status == MediaDecoderJob::DECODE_SUCCEEDED) {
    if (is_audio)
      audio_access_unit_index_++;
    else
      video_access_unit_index_++;
  }

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (is_audio || !HasAudio())
    UpdateTimestamps(presentation_timestamp, wallclock_time);

  if (end_of_stream) {
    PlaybackCompleted(is_audio);
    return;
  }

  if (!playing_)
    return;

  if (is_audio)
    DecodeMoreAudio();
  else
    DecodeMoreVideo();
}

void MediaSourcePlayer::DecodeMoreAudio() {
  if (audio_access_unit_index_ >= received_audio_.access_units.size()) {
    if (!waiting_for_audio_data_) {
      manager()->OnReadFromDemuxer(player_id(), DemuxerStream::AUDIO, true);
      received_audio_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
      audio_access_unit_index_ = 0;
      waiting_for_audio_data_ = true;
    }
    return;
  }

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
      start_wallclock_time_, start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                 weak_this_.GetWeakPtr(), true));
  active_decoding_tasks_++;
}

void MediaSourcePlayer::DecodeMoreVideo() {
  if (video_access_unit_index_ >= received_video_.access_units.size()) {
    if (!waiting_for_video_data_) {
      manager()->OnReadFromDemuxer(player_id(), DemuxerStream::VIDEO, true);
      received_video_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
      video_access_unit_index_ = 0;
      waiting_for_video_data_ = true;
    }
    return;
  }

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

  video_decoder_job_->Decode(
      received_video_.access_units[video_access_unit_index_],
      start_wallclock_time_, start_presentation_timestamp_,
      base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                 weak_this_.GetWeakPtr(), false));
  active_decoding_tasks_++;
}


void MediaSourcePlayer::PlaybackCompleted(bool is_audio) {
  if (is_audio)
    audio_finished_ = true;
  else
    video_finished_ = true;

  if ((!HasAudio() || audio_finished_) && (!HasVideo() || video_finished_)) {
    playing_ = false;
    start_wallclock_time_ = base::Time();
    OnPlaybackComplete();
  }
}

void MediaSourcePlayer::ClearDecodingData() {
  if (audio_decoder_job_)
    audio_decoder_job_->Flush();
  if (video_decoder_job_)
    video_decoder_job_->Flush();
  start_wallclock_time_ = base::Time();
  received_audio_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  received_video_ = MediaPlayerHostMsg_ReadFromDemuxerAck_Params();
  audio_access_unit_index_ = 0;
  video_access_unit_index_ = 0;
}

bool MediaSourcePlayer::HasVideo() {
  return kUnknownVideoCodec != video_codec_;
}

bool MediaSourcePlayer::HasAudio() {
  return kUnknownAudioCodec != audio_codec_;
}

void MediaSourcePlayer::CreateAudioDecoderJob() {
  DCHECK_EQ(0, active_decoding_tasks_);

  // Create audio decoder job only if config changes.
  if (HasAudio() && (reconfig_audio_decoder_ || !audio_decoder_job_)) {
    audio_decoder_job_.reset(new AudioDecoderJob(
        audio_codec_, sampling_rate_, num_channels_,
        &audio_extra_data_[0], audio_extra_data_.size()));
    reconfig_audio_decoder_ =  false;
  }
}

void MediaSourcePlayer::CreateVideoDecoderJob() {
  DCHECK_EQ(0, active_decoding_tasks_);

  if (!HasVideo() || surface_.IsSurfaceEmpty()) {
    video_decoder_job_.reset();
    return;
  }

  if (reconfig_video_decoder_ || !video_decoder_job_) {
    video_decoder_job_.reset();
    video_decoder_job_.reset(new VideoDecoderJob(
        video_codec_, gfx::Size(width_, height_), surface_.j_surface().obj()));
    reconfig_video_decoder_ = false;
  }

  // Inform the fullscreen view the player is ready.
  // TODO(qinmin): refactor MediaPlayerBridge so that we have a better way
  // to inform ContentVideoView.
  OnMediaMetadataChanged(duration_, width_, height_, true);
}

}  // namespace media
