// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_source_player.h"

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/barrier_closure.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/android/audio_decoder_job.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/android/media_player_manager.h"
#include "media/base/android/video_decoder_job.h"
#include "media/base/buffers.h"

namespace media {

MediaSourcePlayer::MediaSourcePlayer(
    int player_id,
    MediaPlayerManager* manager,
    const RequestMediaResourcesCB& request_media_resources_cb,
    const ReleaseMediaResourcesCB& release_media_resources_cb,
    scoped_ptr<DemuxerAndroid> demuxer)
    : MediaPlayerAndroid(player_id,
                         manager,
                         request_media_resources_cb,
                         release_media_resources_cb),
      demuxer_(demuxer.Pass()),
      pending_event_(NO_EVENT_PENDING),
      width_(0),
      height_(0),
      audio_codec_(kUnknownAudioCodec),
      video_codec_(kUnknownVideoCodec),
      num_channels_(0),
      sampling_rate_(0),
      reached_audio_eos_(false),
      reached_video_eos_(false),
      playing_(false),
      is_audio_encrypted_(false),
      is_video_encrypted_(false),
      volume_(-1.0),
      clock_(&default_tick_clock_),
      next_video_data_is_iframe_(true),
      doing_browser_seek_(false),
      pending_seek_(false),
      reconfig_audio_decoder_(false),
      reconfig_video_decoder_(false),
      drm_bridge_(NULL),
      is_waiting_for_key_(false),
      has_pending_audio_data_request_(false),
      has_pending_video_data_request_(false),
      weak_factory_(this) {
  demuxer_->Initialize(this);
  clock_.SetMaxTime(base::TimeDelta());
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
  is_surface_in_use_ = true;

  // If there is a pending surface change event, just wait for it to be
  // processed.
  if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING))
    return;

  // Eventual processing of surface change will take care of feeding the new
  // video decoder initially with I-frame. See b/8950387.
  SetPendingEvent(SURFACE_CHANGE_EVENT_PENDING);

  // If seek is already pending, processing of the pending surface change
  // event will occur in OnDemuxerSeekDone().
  if (IsEventPending(SEEK_EVENT_PENDING))
    return;

  // If video config change is already pending, processing of the pending
  // surface change event will occur in OnDemuxerConfigsAvailable().
  if (reconfig_video_decoder_ && IsEventPending(CONFIG_CHANGE_EVENT_PENDING))
    return;

  // Otherwise we need to trigger pending event processing now.
  ProcessPendingEvents();
}

void MediaSourcePlayer::ScheduleSeekEventAndStopDecoding(
    base::TimeDelta seek_time) {
  DVLOG(1) << __FUNCTION__ << "(" << seek_time.InSecondsF() << ")";
  DCHECK(!IsEventPending(SEEK_EVENT_PENDING));

  pending_seek_ = false;

  clock_.SetTime(seek_time, seek_time);

  if (audio_decoder_job_ && audio_decoder_job_->is_decoding())
    audio_decoder_job_->StopDecode();
  if (video_decoder_job_ && video_decoder_job_->is_decoding())
    video_decoder_job_->StopDecode();

  SetPendingEvent(SEEK_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::BrowserSeekToCurrentTime() {
  DVLOG(1) << __FUNCTION__;

  DCHECK(!IsEventPending(SEEK_EVENT_PENDING));
  doing_browser_seek_ = true;
  ScheduleSeekEventAndStopDecoding(GetCurrentTime());
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
  DVLOG(1) << __FUNCTION__;

  playing_ = true;

  if (IsProtectedSurfaceRequired())
    manager()->OnProtectedSurfaceRequested(player_id());

  StartInternal();
}

void MediaSourcePlayer::Pause(bool is_media_related_action) {
  DVLOG(1) << __FUNCTION__;

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
  DVLOG(1) << __FUNCTION__ << "(" << timestamp.InSecondsF() << ")";

  if (IsEventPending(SEEK_EVENT_PENDING)) {
    DCHECK(doing_browser_seek_) << "SeekTo while SeekTo in progress";
    DCHECK(!pending_seek_) << "SeekTo while SeekTo pending browser seek";

    // There is a browser seek currently in progress to obtain I-frame to feed
    // a newly constructed video decoder. Remember this real seek request so
    // it can be initiated once OnDemuxerSeekDone() occurs for the browser seek.
    pending_seek_ = true;
    pending_seek_time_ = timestamp;
    return;
  }

  doing_browser_seek_ = false;
  ScheduleSeekEventAndStopDecoding(timestamp);
}

base::TimeDelta MediaSourcePlayer::GetCurrentTime() {
  return clock_.Elapsed();
}

base::TimeDelta MediaSourcePlayer::GetDuration() {
  return duration_;
}

void MediaSourcePlayer::Release() {
  DVLOG(1) << __FUNCTION__;

  // Allow pending seeks and config changes to survive this Release().
  // If previously pending a prefetch done event, or a job was still decoding,
  // then at end of Release() we need to ProcessPendingEvents() to process any
  // seek or config change that was blocked by the prefetch or decode.
  // TODO(qinmin/wolenetz): Maintain channel state to not double-request data
  // or drop data received across Release()+Start(). See http://crbug.com/306314
  // and http://crbug.com/304234.
  bool process_pending_events = false;
  process_pending_events = IsEventPending(PREFETCH_DONE_EVENT_PENDING) ||
      (audio_decoder_job_ && audio_decoder_job_->is_decoding()) ||
      (video_decoder_job_ && video_decoder_job_->is_decoding());

  // Clear all the pending events except seeks and config changes.
  pending_event_ &= (SEEK_EVENT_PENDING | CONFIG_CHANGE_EVENT_PENDING);
  is_surface_in_use_ = false;
  ResetAudioDecoderJob();
  ResetVideoDecoderJob();

  // Prevent job re-creation attempts in OnDemuxerConfigsAvailable()
  reconfig_audio_decoder_ = false;
  reconfig_video_decoder_ = false;

  // Prevent player restart, including job re-creation attempts.
  playing_ = false;

  decoder_starvation_callback_.Cancel();
  surface_ = gfx::ScopedJavaSurface();
  if (process_pending_events) {
    DVLOG(1) << __FUNCTION__ << " : Resuming seek or config change processing";
    ProcessPendingEvents();
  }
}

void MediaSourcePlayer::SetVolume(double volume) {
  volume_ = volume;
  SetVolumeInternal();
}

void MediaSourcePlayer::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;
  if (!is_waiting_for_key_)
    return;

  is_waiting_for_key_ = false;
  if (playing_)
    StartInternal();
}

bool MediaSourcePlayer::IsSurfaceInUse() const {
  return is_surface_in_use_;
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
  DVLOG(1) << __FUNCTION__;
  // If there are pending events, wait for them finish.
  if (pending_event_ != NO_EVENT_PENDING)
    return;

  // When we start, we'll have new demuxed data coming in. This new data could
  // be clear (not encrypted) or encrypted with different keys. So
  // |is_waiting_for_key_| condition may not be true anymore.
  is_waiting_for_key_ = false;

  // Create decoder jobs if they are not created
  ConfigureAudioDecoderJob();
  ConfigureVideoDecoderJob();

  // If one of the decoder job is not ready, do nothing.
  if ((HasAudio() && !audio_decoder_job_) ||
      (HasVideo() && !video_decoder_job_)) {
    return;
  }

  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::OnDemuxerConfigsAvailable(
    const DemuxerConfigs& configs) {
  DVLOG(1) << __FUNCTION__;
  duration_ = base::TimeDelta::FromMilliseconds(configs.duration_ms);
  clock_.SetDuration(duration_);

  audio_codec_ = configs.audio_codec;
  num_channels_ = configs.audio_channels;
  sampling_rate_ = configs.audio_sampling_rate;
  is_audio_encrypted_ = configs.is_audio_encrypted;
  audio_extra_data_ = configs.audio_extra_data;
  video_codec_ = configs.video_codec;
  width_ = configs.video_size.width();
  height_ = configs.video_size.height();
  is_video_encrypted_ = configs.is_video_encrypted;

  manager()->OnMediaMetadataChanged(
      player_id(), duration_, width_, height_, true);

  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    if (reconfig_audio_decoder_)
      ConfigureAudioDecoderJob();

    if (reconfig_video_decoder_)
      ConfigureVideoDecoderJob();

    ClearPendingEvent(CONFIG_CHANGE_EVENT_PENDING);

    // Resume decoding after the config change if we are still playing.
    if (playing_)
      StartInternal();
  }
}

void MediaSourcePlayer::OnDemuxerDataAvailable(const DemuxerData& data) {
  DVLOG(1) << __FUNCTION__ << "(" << data.type << ")";
  DCHECK_LT(0u, data.access_units.size());

  if (has_pending_audio_data_request_ && data.type == DemuxerStream::AUDIO) {
    has_pending_audio_data_request_ = false;
    ProcessPendingEvents();
    return;
  }

  if (has_pending_video_data_request_ && data.type == DemuxerStream::VIDEO) {
    next_video_data_is_iframe_ = false;
    has_pending_video_data_request_ = false;
    ProcessPendingEvents();
    return;
  }

  if (data.type == DemuxerStream::AUDIO && audio_decoder_job_) {
    audio_decoder_job_->OnDataReceived(data);
  } else if (data.type == DemuxerStream::VIDEO) {
    next_video_data_is_iframe_ = false;
    if (video_decoder_job_)
      video_decoder_job_->OnDataReceived(data);
  }
}

void MediaSourcePlayer::OnDemuxerDurationChanged(base::TimeDelta duration) {
  duration_ = duration;
  clock_.SetDuration(duration_);
}

base::android::ScopedJavaLocalRef<jobject> MediaSourcePlayer::GetMediaCrypto() {
  base::android::ScopedJavaLocalRef<jobject> media_crypto;
  if (drm_bridge_)
    media_crypto = drm_bridge_->GetMediaCrypto();
  return media_crypto;
}

void MediaSourcePlayer::OnMediaCryptoReady() {
  DCHECK(!drm_bridge_->GetMediaCrypto().is_null());
  drm_bridge_->SetMediaCryptoReadyCB(base::Closure());

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::SetDrmBridge(MediaDrmBridge* drm_bridge) {
  // Currently we don't support DRM change during the middle of playback, even
  // if the player is paused.
  // TODO(qinmin): support DRM change after playback has started.
  // http://crbug.com/253792.
  if (GetCurrentTime() > base::TimeDelta()) {
    VLOG(0) << "Setting DRM bridge after playback has started. "
            << "This is not well supported!";
  }

  drm_bridge_ = drm_bridge;

  if (drm_bridge_->GetMediaCrypto().is_null()) {
    drm_bridge_->SetMediaCryptoReadyCB(base::Bind(
        &MediaSourcePlayer::OnMediaCryptoReady, weak_factory_.GetWeakPtr()));
    return;
  }

  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::OnDemuxerSeekDone(
    base::TimeDelta actual_browser_seek_time) {
  DVLOG(1) << __FUNCTION__;

  ClearPendingEvent(SEEK_EVENT_PENDING);
  if (IsEventPending(PREFETCH_REQUEST_EVENT_PENDING))
    ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);

  next_video_data_is_iframe_ = true;

  if (pending_seek_) {
    DVLOG(1) << __FUNCTION__ << "processing pending seek";
    DCHECK(doing_browser_seek_);
    pending_seek_ = false;
    SeekTo(pending_seek_time_);
    return;
  }

  // It is possible that a browser seek to I-frame had to seek to a buffered
  // I-frame later than the requested one due to data removal or GC. Update
  // player clock to the actual seek target.
  if (doing_browser_seek_) {
    DCHECK(actual_browser_seek_time != kNoTimestamp());
    base::TimeDelta seek_time = actual_browser_seek_time;
    // A browser seek must not jump into the past. Ideally, it seeks to the
    // requested time, but it might jump into the future.
    DCHECK(seek_time >= GetCurrentTime());
    DVLOG(1) << __FUNCTION__ << " : setting clock to actual browser seek time: "
             << seek_time.InSecondsF();
    clock_.SetTime(seek_time, seek_time);
    if (audio_decoder_job_)
      audio_decoder_job_->SetBaseTimestamp(seek_time);
  } else {
    DCHECK(actual_browser_seek_time == kNoTimestamp());
  }

  reached_audio_eos_ = false;
  reached_video_eos_ = false;

  base::TimeDelta current_time = GetCurrentTime();
  // TODO(qinmin): Simplify the logic by using |start_presentation_timestamp_|
  // to preroll media decoder jobs. Currently |start_presentation_timestamp_|
  // is calculated from decoder output, while preroll relies on the access
  // unit's timestamp. There are some differences between the two.
  preroll_timestamp_ = current_time;
  if (audio_decoder_job_)
    audio_decoder_job_->BeginPrerolling(preroll_timestamp_);
  if (video_decoder_job_)
    video_decoder_job_->BeginPrerolling(preroll_timestamp_);

  if (!doing_browser_seek_)
    manager()->OnSeekComplete(player_id(), current_time);

  ProcessPendingEvents();
}

void MediaSourcePlayer::UpdateTimestamps(
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  clock_.SetTime(current_presentation_timestamp, max_presentation_timestamp);

  manager()->OnTimeUpdate(player_id(), GetCurrentTime());
}

void MediaSourcePlayer::ProcessPendingEvents() {
  DVLOG(1) << __FUNCTION__ << " : 0x" << std::hex << pending_event_;
  // Wait for all the decoding jobs to finish before processing pending tasks.
  if (video_decoder_job_ && video_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : A video job is still decoding.";
    return;
  }

  if (audio_decoder_job_ && audio_decoder_job_->is_decoding()) {
    DVLOG(1) << __FUNCTION__ << " : An audio job is still decoding.";
    return;
  }

  if (has_pending_audio_data_request_ || has_pending_video_data_request_) {
    DVLOG(1) << __FUNCTION__ << " : has pending data request.";
    return;
  }

  if (IsEventPending(PREFETCH_DONE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : PREFETCH_DONE still pending.";
    return;
  }

  if (IsEventPending(SEEK_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling SEEK_EVENT";
    ClearDecodingData();
    if (audio_decoder_job_)
      audio_decoder_job_->SetBaseTimestamp(GetCurrentTime());
    demuxer_->RequestDemuxerSeek(GetCurrentTime(), doing_browser_seek_);
    return;
  }

  start_time_ticks_ = base::TimeTicks();
  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling CONFIG_CHANGE_EVENT.";
    DCHECK(reconfig_audio_decoder_ || reconfig_video_decoder_);
    demuxer_->RequestDemuxerConfigs();
    return;
  }

  if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling SURFACE_CHANGE_EVENT.";
    // Setting a new surface will require a new MediaCodec to be created.
    ResetVideoDecoderJob();
    ConfigureVideoDecoderJob();

    // Return early if we can't successfully configure a new video decoder job
    // yet.
    if (HasVideo() && !video_decoder_job_)
      return;
  }

  if (IsEventPending(PREFETCH_REQUEST_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : Handling PREFETCH_REQUEST_EVENT.";
    // If one of the decoder is not initialized, cancel this event as it will be
    // called later when Start() is called again.
    if ((HasVideo() && !video_decoder_job_) ||
        (HasAudio() && !audio_decoder_job_)) {
      ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
      return;
    }

    DCHECK(audio_decoder_job_ || AudioFinished());
    DCHECK(video_decoder_job_ || VideoFinished());

    int count = (AudioFinished() ? 0 : 1) + (VideoFinished() ? 0 : 1);

    // It is possible that all streams have finished decode, yet starvation
    // occurred during the last stream's EOS decode. In this case, prefetch is a
    // no-op.
    ClearPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
    if (count == 0)
      return;

    SetPendingEvent(PREFETCH_DONE_EVENT_PENDING);
    base::Closure barrier =
        BarrierClosure(count,
                       base::Bind(&MediaSourcePlayer::OnPrefetchDone,
                                  weak_factory_.GetWeakPtr()));

    if (!AudioFinished())
      audio_decoder_job_->Prefetch(barrier);

    if (!VideoFinished())
      video_decoder_job_->Prefetch(barrier);

    return;
  }

  DCHECK_EQ(pending_event_, NO_EVENT_PENDING);

  // Now that all pending events have been handled, resume decoding if we are
  // still playing.
  if (playing_)
    StartInternal();
}

void MediaSourcePlayer::MediaDecoderCallback(
    bool is_audio, MediaCodecStatus status,
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  DVLOG(1) << __FUNCTION__ << ": " << is_audio << ", " << status;

  // TODO(xhwang): Drop IntToString() when http://crbug.com/303899 is fixed.
  if (is_audio) {
    TRACE_EVENT_ASYNC_END1("media",
                           "MediaSourcePlayer::DecodeMoreAudio",
                           audio_decoder_job_.get(),
                           "MediaCodecStatus",
                           base::IntToString(status));
  } else {
    TRACE_EVENT_ASYNC_END1("media",
                           "MediaSourcePlayer::DecodeMoreVideo",
                           video_decoder_job_.get(),
                           "MediaCodecStatus",
                           base::IntToString(status));
  }

  // Let tests hook the completion of this decode cycle.
  if (!decode_callback_for_testing_.is_null())
    base::ResetAndReturn(&decode_callback_for_testing_).Run();

  bool is_clock_manager = is_audio || !HasAudio();

  if (is_clock_manager)
    decoder_starvation_callback_.Cancel();

  if (status == MEDIA_CODEC_ERROR) {
    DVLOG(1) << __FUNCTION__ << " : decode error";
    Release();
    manager()->OnError(player_id(), MEDIA_ERROR_DECODE);
    return;
  }

  DCHECK(!IsEventPending(PREFETCH_DONE_EVENT_PENDING));

  // Let |SEEK_EVENT_PENDING| (the highest priority event outside of
  // |PREFETCH_DONE_EVENT_PENDING|) preempt output EOS detection here. Process
  // any other pending events only after handling EOS detection.
  if (IsEventPending(SEEK_EVENT_PENDING)) {
    ProcessPendingEvents();
    return;
  }

  if (status == MEDIA_CODEC_OK && is_clock_manager &&
      current_presentation_timestamp != kNoTimestamp()) {
    UpdateTimestamps(
        current_presentation_timestamp, max_presentation_timestamp);
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM)
    PlaybackCompleted(is_audio);

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  if (status == MEDIA_CODEC_OUTPUT_END_OF_STREAM)
    return;

  if (!playing_) {
    if (is_clock_manager)
      clock_.Pause();
    return;
  }

  if (status == MEDIA_CODEC_NO_KEY) {
    is_waiting_for_key_ = true;
    return;
  }

  // If the status is MEDIA_CODEC_STOPPED, stop decoding new data. The player is
  // in the middle of a seek or stop event and needs to wait for the IPCs to
  // come.
  if (status == MEDIA_CODEC_STOPPED)
    return;

  if (is_clock_manager) {
    // If we have a valid timestamp, start the starvation callback. Otherwise,
    // reset the |start_time_ticks_| so that the next frame will not suffer
    // from the decoding delay caused by the current frame.
    if (current_presentation_timestamp != kNoTimestamp())
      StartStarvationCallback(current_presentation_timestamp,
                              max_presentation_timestamp);
    else
      start_time_ticks_ = base::TimeTicks::Now();
  }

  if (is_audio) {
    DecodeMoreAudio();
    return;
  }

  DecodeMoreVideo();
}

void MediaSourcePlayer::DecodeMoreAudio() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_->is_decoding());
  DCHECK(!AudioFinished());

  if (audio_decoder_job_->Decode(
          start_time_ticks_,
          start_presentation_timestamp_,
          base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                     weak_factory_.GetWeakPtr(),
                     true))) {
    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSourcePlayer::DecodeMoreAudio",
                             audio_decoder_job_.get());
    return;
  }

  // Failed to start the next decode.
  // Wait for demuxer ready message.
  DCHECK(!reconfig_audio_decoder_);
  reconfig_audio_decoder_ = true;

  // Config change may have just been detected on the other stream. If so,
  // don't send a duplicate demuxer config request.
  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    DCHECK(reconfig_video_decoder_);
    return;
  }

  SetPendingEvent(CONFIG_CHANGE_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::DecodeMoreVideo() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!video_decoder_job_->is_decoding());
  DCHECK(!VideoFinished());

  if (video_decoder_job_->Decode(
          start_time_ticks_,
          start_presentation_timestamp_,
          base::Bind(&MediaSourcePlayer::MediaDecoderCallback,
                     weak_factory_.GetWeakPtr(),
                     false))) {
    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSourcePlayer::DecodeMoreVideo",
                             video_decoder_job_.get());
    return;
  }

  // Failed to start the next decode.
  // Wait for demuxer ready message.

  // After this detection of video config change, next video data received
  // will begin with I-frame.
  next_video_data_is_iframe_ = true;

  DCHECK(!reconfig_video_decoder_);
  reconfig_video_decoder_ = true;

  // Config change may have just been detected on the other stream. If so,
  // don't send a duplicate demuxer config request.
  if (IsEventPending(CONFIG_CHANGE_EVENT_PENDING)) {
    DCHECK(reconfig_audio_decoder_);
    return;
  }

  SetPendingEvent(CONFIG_CHANGE_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::PlaybackCompleted(bool is_audio) {
  DVLOG(1) << __FUNCTION__ << "(" << is_audio << ")";
  if (is_audio)
    reached_audio_eos_ = true;
  else
    reached_video_eos_ = true;

  if (AudioFinished() && VideoFinished()) {
    playing_ = false;
    clock_.Pause();
    start_time_ticks_ = base::TimeTicks();
    manager()->OnPlaybackComplete(player_id());
  }
}

void MediaSourcePlayer::ClearDecodingData() {
  DVLOG(1) << __FUNCTION__;
  if (audio_decoder_job_)
    audio_decoder_job_->Flush();
  if (video_decoder_job_)
    video_decoder_job_->Flush();
  start_time_ticks_ = base::TimeTicks();
}

bool MediaSourcePlayer::HasVideo() {
  return kUnknownVideoCodec != video_codec_;
}

bool MediaSourcePlayer::HasAudio() {
  return kUnknownAudioCodec != audio_codec_;
}

bool MediaSourcePlayer::AudioFinished() {
  return reached_audio_eos_ || !HasAudio();
}

bool MediaSourcePlayer::VideoFinished() {
  return reached_video_eos_ || !HasVideo();
}

void MediaSourcePlayer::ConfigureAudioDecoderJob() {
  if (!HasAudio()) {
    ResetAudioDecoderJob();
    return;
  }

  // Create audio decoder job only if config changes.
  if (audio_decoder_job_ && !reconfig_audio_decoder_)
    return;

  base::android::ScopedJavaLocalRef<jobject> media_crypto = GetMediaCrypto();
  if (is_audio_encrypted_ && media_crypto.is_null())
    return;

  DCHECK(!audio_decoder_job_ || !audio_decoder_job_->is_decoding());

  ResetAudioDecoderJob();
  DVLOG(1) << __FUNCTION__ << " : creating new audio decoder job";
  audio_decoder_job_.reset(AudioDecoderJob::Create(
      audio_codec_, sampling_rate_, num_channels_, &audio_extra_data_[0],
      audio_extra_data_.size(), media_crypto.obj(),
      base::Bind(&DemuxerAndroid::RequestDemuxerData,
                 base::Unretained(demuxer_.get()), DemuxerStream::AUDIO)));

  if (audio_decoder_job_) {
    SetVolumeInternal();
    // Need to reset the base timestamp in |audio_decoder_job_|.
    // TODO(qinmin): When reconfiguring the |audio_decoder_job_|, there might
    // still be some audio frames in the decoder or in AudioTrack. Therefore,
    // we are losing some time here. http://crbug.com/357726.
    base::TimeDelta current_time = GetCurrentTime();
    audio_decoder_job_->SetBaseTimestamp(current_time);
    clock_.SetTime(current_time, current_time);
    audio_decoder_job_->BeginPrerolling(preroll_timestamp_);
    reconfig_audio_decoder_ =  false;
  }
}

void MediaSourcePlayer::ResetVideoDecoderJob() {
  if (video_decoder_job_) {
    has_pending_video_data_request_ =
        video_decoder_job_->is_requesting_demuxer_data();
  }
  video_decoder_job_.reset();

  // Any eventual video decoder job re-creation will use the current |surface_|.
  if (IsEventPending(SURFACE_CHANGE_EVENT_PENDING))
    ClearPendingEvent(SURFACE_CHANGE_EVENT_PENDING);
}

void MediaSourcePlayer::ResetAudioDecoderJob() {
  if (audio_decoder_job_) {
    has_pending_audio_data_request_ =
        audio_decoder_job_->is_requesting_demuxer_data();
  }
  audio_decoder_job_.reset();
}

void MediaSourcePlayer::ConfigureVideoDecoderJob() {
  if (!HasVideo() || surface_.IsEmpty()) {
    ResetVideoDecoderJob();
    return;
  }

  // Create video decoder job only if config changes or we don't have a job.
  if (video_decoder_job_ && !reconfig_video_decoder_) {
    DCHECK(!IsEventPending(SURFACE_CHANGE_EVENT_PENDING));
    return;
  }

  DCHECK(!video_decoder_job_ || !video_decoder_job_->is_decoding());

  if (reconfig_video_decoder_) {
    // No hack browser seek should be required. I-Frame must be next.
    DCHECK(next_video_data_is_iframe_) << "Received video data between "
        << "detecting video config change and reconfiguring video decoder";
  }

  // If uncertain that video I-frame data is next and there is no seek already
  // in process, request browser demuxer seek so the new decoder will decode
  // an I-frame first. Otherwise, the new MediaCodec might crash. See b/8950387.
  // Eventual OnDemuxerSeekDone() will trigger ProcessPendingEvents() and
  // continue from here.
  // TODO(wolenetz): Instead of doing hack browser seek, replay cached data
  // since last keyframe. See http://crbug.com/304234.
  if (!next_video_data_is_iframe_ && !IsEventPending(SEEK_EVENT_PENDING)) {
    BrowserSeekToCurrentTime();
    return;
  }

  // Release the old VideoDecoderJob first so the surface can get released.
  // Android does not allow 2 MediaCodec instances use the same surface.
  ResetVideoDecoderJob();

  base::android::ScopedJavaLocalRef<jobject> media_crypto = GetMediaCrypto();
  if (is_video_encrypted_ && media_crypto.is_null())
    return;

  DVLOG(1) << __FUNCTION__ << " : creating new video decoder job";

  // Create the new VideoDecoderJob.
  bool is_secure = IsProtectedSurfaceRequired();
  video_decoder_job_.reset(
      VideoDecoderJob::Create(
          video_codec_,
          is_secure,
          gfx::Size(width_, height_),
          surface_.j_surface().obj(),
          media_crypto.obj(),
          base::Bind(&DemuxerAndroid::RequestDemuxerData,
                     base::Unretained(demuxer_.get()),
                     DemuxerStream::VIDEO),
          base::Bind(request_media_resources_cb_, player_id()),
          base::Bind(release_media_resources_cb_, player_id())));
  if (!video_decoder_job_)
    return;

  video_decoder_job_->BeginPrerolling(preroll_timestamp_);
  reconfig_video_decoder_ = false;

  // Inform the fullscreen view the player is ready.
  // TODO(qinmin): refactor MediaPlayerBridge so that we have a better way
  // to inform ContentVideoView.
  manager()->OnMediaMetadataChanged(
      player_id(), duration_, width_, height_, true);
}

void MediaSourcePlayer::OnDecoderStarved() {
  DVLOG(1) << __FUNCTION__;
  SetPendingEvent(PREFETCH_REQUEST_EVENT_PENDING);
  ProcessPendingEvents();
}

void MediaSourcePlayer::StartStarvationCallback(
    base::TimeDelta current_presentation_timestamp,
    base::TimeDelta max_presentation_timestamp) {
  // 20ms was chosen because it is the typical size of a compressed audio frame.
  // Anything smaller than this would likely cause unnecessary cycling in and
  // out of the prefetch state.
  const base::TimeDelta kMinStarvationTimeout =
      base::TimeDelta::FromMilliseconds(20);

  base::TimeDelta current_timestamp = GetCurrentTime();
  base::TimeDelta timeout;
  if (HasAudio()) {
    timeout = max_presentation_timestamp - current_timestamp;
  } else {
    DCHECK(current_timestamp <= current_presentation_timestamp);

    // For video only streams, fps can be estimated from the difference
    // between the previous and current presentation timestamps. The
    // previous presentation timestamp is equal to current_timestamp.
    // TODO(qinmin): determine whether 2 is a good coefficient for estimating
    // video frame timeout.
    timeout = 2 * (current_presentation_timestamp - current_timestamp);
  }

  timeout = std::max(timeout, kMinStarvationTimeout);

  decoder_starvation_callback_.Reset(base::Bind(
      &MediaSourcePlayer::OnDecoderStarved, weak_factory_.GetWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE, decoder_starvation_callback_.callback(), timeout);
}

void MediaSourcePlayer::SetVolumeInternal() {
  if (audio_decoder_job_ && volume_ >= 0)
    audio_decoder_job_->SetVolume(volume_);
}

bool MediaSourcePlayer::IsProtectedSurfaceRequired() {
  return is_video_encrypted_ &&
      drm_bridge_ && drm_bridge_->IsProtectedSurfaceRequired();
}

void MediaSourcePlayer::OnPrefetchDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(!audio_decoder_job_ || !audio_decoder_job_->is_decoding());
  DCHECK(!video_decoder_job_ || !video_decoder_job_->is_decoding());

  // A previously posted OnPrefetchDone() could race against a Release(). If
  // Release() won the race, we should no longer have decoder jobs.
  // TODO(qinmin/wolenetz): Maintain channel state to not double-request data
  // or drop data received across Release()+Start(). See http://crbug.com/306314
  // and http://crbug.com/304234.
  if (!IsEventPending(PREFETCH_DONE_EVENT_PENDING)) {
    DVLOG(1) << __FUNCTION__ << " : aborting";
    DCHECK(!audio_decoder_job_ && !video_decoder_job_);
    return;
  }

  ClearPendingEvent(PREFETCH_DONE_EVENT_PENDING);

  if (pending_event_ != NO_EVENT_PENDING) {
    ProcessPendingEvents();
    return;
  }

  start_time_ticks_ = base::TimeTicks::Now();
  start_presentation_timestamp_ = GetCurrentTime();
  if (!clock_.IsPlaying())
    clock_.Play();

  if (!AudioFinished())
    DecodeMoreAudio();

  if (!VideoFinished())
    DecodeMoreVideo();
}

const char* MediaSourcePlayer::GetEventName(PendingEventFlags event) {
  static const char* kPendingEventNames[] = {
    "SEEK",
    "SURFACE_CHANGE",
    "CONFIG_CHANGE",
    "PREFETCH_REQUEST",
    "PREFETCH_DONE",
  };

  int mask = 1;
  for (size_t i = 0; i < arraysize(kPendingEventNames); ++i, mask <<= 1) {
    if (event & mask)
      return kPendingEventNames[i];
  }

  return "UNKNOWN";
}

bool MediaSourcePlayer::IsEventPending(PendingEventFlags event) const {
  return pending_event_ & event;
}

void MediaSourcePlayer::SetPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(!IsEventPending(event));

  pending_event_ |= event;
}

void MediaSourcePlayer::ClearPendingEvent(PendingEventFlags event) {
  DVLOG(1) << __FUNCTION__ << "(" << GetEventName(event) << ")";
  DCHECK_NE(event, NO_EVENT_PENDING);
  DCHECK(IsEventPending(event)) << GetEventName(event);

  pending_event_ &= ~event;
}

}  // namespace media
