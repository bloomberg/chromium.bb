// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "media/base/android/audio_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/video_decoder_job.h"
#include "media/base/audio_timestamp_helper.h"

namespace {

// Use 16bit PCM for audio output. Keep this value in sync with the output
// format we passed to AudioTrack in MediaCodecBridge.
const int kBytesPerAudioOutputSample = 2;
}

namespace media {

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
      volume_(-1.0),
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
  // For an empty surface, always pass it to the decoder job so that it
  // can detach from the current one. Otherwise, don't pass an unprotected
  // surface if the video content requires a protected one.
  if (!surface.IsEmpty() &&
      IsProtectedSurfaceRequired() && !surface.is_protected()) {
    return;
  }

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

  if (IsProtectedSurfaceRequired())
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

void MediaSourcePlayer::SetVolume(double volume) {
  volume_ = volume;
  SetVolumeInternal();
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

  // If the input reaches input EOS, there is no need to request new data.
  if (decode_status != MediaDecoderJob::DECODE_TRY_ENQUEUE_INPUT_AGAIN_LATER &&
      decode_status != MediaDecoderJob::DECODE_INPUT_END_OF_STREAM) {
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

  if (decode_status == MediaDecoderJob::DECODE_OUTPUT_END_OF_STREAM) {
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

  if (audio_decoder_job_) {
    SetVolumeInternal();
    reconfig_audio_decoder_ =  false;
  }
}

void MediaSourcePlayer::ConfigureVideoDecoderJob() {
  if (!HasVideo() || surface_.IsEmpty()) {
    video_decoder_job_.reset();
    return;
  }

  // Create video decoder job only if config changes.
  if (video_decoder_job_ && !reconfig_video_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_crypto;
  if (is_video_encrypted_) {
    if (drm_bridge_) {
      media_crypto = drm_bridge_->GetMediaCrypto();
      DCHECK(!media_crypto.is_null());
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
      media_crypto.obj()));
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

void MediaSourcePlayer::SetVolumeInternal() {
  if (audio_decoder_job_ && volume_ >= 0)
    audio_decoder_job_.get()->SetVolume(volume_);
}

bool MediaSourcePlayer::IsProtectedSurfaceRequired() {
  return is_video_encrypted_ &&
      drm_bridge_ && drm_bridge_->IsProtectedSurfaceRequired();
}

}  // namespace media
