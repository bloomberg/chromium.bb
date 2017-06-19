// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/watch_time_reporter.h"

#include "base/power_monitor/power_monitor.h"
#include "media/base/watch_time_keys.h"

namespace media {

// The minimum amount of media playback which can elapse before we'll report
// watch time metrics for a playback.
constexpr base::TimeDelta kMinimumElapsedWatchTime =
    base::TimeDelta::FromSeconds(7);

// The minimum width and height of videos to report watch time metrics for.
constexpr gfx::Size kMinimumVideoSize = gfx::Size(200, 200);

static bool IsOnBatteryPower() {
  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    return pm->IsOnBatteryPower();
  return false;
}

WatchTimeReporter::WatchTimeReporter(bool has_audio,
                                     bool has_video,
                                     bool is_mse,
                                     bool is_encrypted,
                                     bool is_embedded_media_experience_enabled,
                                     MediaLog* media_log,
                                     const gfx::Size& initial_video_size,
                                     const GetMediaTimeCB& get_media_time_cb)
    : WatchTimeReporter(has_audio,
                        has_video,
                        is_mse,
                        is_encrypted,
                        is_embedded_media_experience_enabled,
                        media_log,
                        initial_video_size,
                        get_media_time_cb,
                        false) {}

WatchTimeReporter::WatchTimeReporter(bool has_audio,
                                     bool has_video,
                                     bool is_mse,
                                     bool is_encrypted,
                                     bool is_embedded_media_experience_enabled,
                                     MediaLog* media_log,
                                     const gfx::Size& initial_video_size,
                                     const GetMediaTimeCB& get_media_time_cb,
                                     bool is_background)
    : has_audio_(has_audio),
      has_video_(has_video),
      is_mse_(is_mse),
      is_encrypted_(is_encrypted),
      is_embedded_media_experience_enabled_(
          is_embedded_media_experience_enabled),
      media_log_(media_log),
      initial_video_size_(initial_video_size),
      get_media_time_cb_(get_media_time_cb),
      is_background_(is_background) {
  DCHECK(!get_media_time_cb_.is_null());
  DCHECK(has_audio_ || has_video_);

  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    pm->AddObserver(this);

  if (is_background_) {
    DCHECK(has_audio_);
    DCHECK(!has_video_);
    return;
  }

  // Background watch time is reported by creating an audio only watch time
  // reporter which receives play when hidden and pause when shown. This avoids
  // unnecessary complexity inside the UpdateWatchTime() for handling this case.
  if (has_video_ && has_audio_ && ShouldReportWatchTime()) {
    background_reporter_.reset(
        new WatchTimeReporter(has_audio_, false, is_mse_, is_encrypted_,
                              is_embedded_media_experience_enabled_, media_log_,
                              initial_video_size_, get_media_time_cb_, true));
  }
}

WatchTimeReporter::~WatchTimeReporter() {
  background_reporter_.reset();

  // If the timer is still running, finalize immediately, this is our last
  // chance to capture metrics.
  if (reporting_timer_.IsRunning())
    MaybeFinalizeWatchTime(FinalizeTime::IMMEDIATELY);

  if (base::PowerMonitor* pm = base::PowerMonitor::Get())
    pm->RemoveObserver(this);
}

void WatchTimeReporter::OnPlaying() {
  if (background_reporter_ && !is_visible_)
    background_reporter_->OnPlaying();

  is_playing_ = true;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnPaused() {
  if (background_reporter_)
    background_reporter_->OnPaused();

  is_playing_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnSeeking() {
  if (background_reporter_)
    background_reporter_->OnSeeking();

  if (!reporting_timer_.IsRunning())
    return;

  // Seek is a special case that does not have hysteresis, when this is called
  // the seek is imminent, so finalize the previous playback immediately.

  // Don't trample an existing end timestamp.
  if (end_timestamp_ == kNoTimestamp)
    end_timestamp_ = get_media_time_cb_.Run();
  UpdateWatchTime();
}

void WatchTimeReporter::OnVolumeChange(double volume) {
  if (background_reporter_)
    background_reporter_->OnVolumeChange(volume);

  const double old_volume = volume_;
  volume_ = volume;

  // We're only interesting in transitions in and out of the muted state.
  if (!old_volume && volume)
    MaybeStartReportingTimer(get_media_time_cb_.Run());
  else if (old_volume && !volume_)
    MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

void WatchTimeReporter::OnShown() {
  if (background_reporter_)
    background_reporter_->OnPaused();

  if (!has_video_)
    return;

  is_visible_ = true;
  MaybeStartReportingTimer(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnHidden() {
  if (background_reporter_ && is_playing_)
    background_reporter_->OnPlaying();

  if (!has_video_)
    return;

  is_visible_ = false;
  MaybeFinalizeWatchTime(FinalizeTime::ON_NEXT_UPDATE);
}

bool WatchTimeReporter::IsSizeLargeEnoughToReportWatchTime() const {
  return initial_video_size_.height() >= kMinimumVideoSize.height() &&
         initial_video_size_.width() >= kMinimumVideoSize.width();
}

void WatchTimeReporter::OnUnderflow() {
  if (!reporting_timer_.IsRunning())
    return;

  // In the event of a pending finalize, we don't want to count underflow events
  // that occurred after the finalize time. Yet if the finalize is canceled we
  // want to ensure they are all recorded.
  pending_underflow_events_.push_back(get_media_time_cb_.Run());
}

void WatchTimeReporter::OnNativeControlsEnabled() {
  if (!reporting_timer_.IsRunning()) {
    has_native_controls_ = true;
    return;
  }

  if (end_timestamp_for_controls_ != kNoTimestamp) {
    end_timestamp_for_controls_ = kNoTimestamp;
    return;
  }

  end_timestamp_for_controls_ = get_media_time_cb_.Run();
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::OnNativeControlsDisabled() {
  if (!reporting_timer_.IsRunning()) {
    has_native_controls_ = false;
    return;
  }

  if (end_timestamp_for_controls_ != kNoTimestamp) {
    end_timestamp_for_controls_ = kNoTimestamp;
    return;
  }

  end_timestamp_for_controls_ = get_media_time_cb_.Run();
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::OnDisplayTypeInline() {
  OnDisplayTypeChanged(blink::WebMediaPlayer::DisplayType::kInline);
}

void WatchTimeReporter::OnDisplayTypeFullscreen() {
  OnDisplayTypeChanged(blink::WebMediaPlayer::DisplayType::kFullscreen);
}

void WatchTimeReporter::OnDisplayTypePictureInPicture() {
  OnDisplayTypeChanged(blink::WebMediaPlayer::DisplayType::kPictureInPicture);
}

void WatchTimeReporter::OnPowerStateChange(bool on_battery_power) {
  if (!reporting_timer_.IsRunning())
    return;

  // Defer changing |is_on_battery_power_| until the next watch time report to
  // avoid momentary power changes from affecting the results.
  if (is_on_battery_power_ != on_battery_power) {
    end_timestamp_for_power_ = get_media_time_cb_.Run();

    // Restart the reporting timer so the full hysteresis is afforded.
    reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                           &WatchTimeReporter::UpdateWatchTime);
    return;
  }

  end_timestamp_for_power_ = kNoTimestamp;
}

bool WatchTimeReporter::ShouldReportWatchTime() {
  // Report listen time or watch time only for tracks that are audio-only or
  // have both an audio and video track of sufficient size.
  return (!has_video_ && has_audio_) ||
         (has_video_ && has_audio_ && IsSizeLargeEnoughToReportWatchTime());
}

void WatchTimeReporter::MaybeStartReportingTimer(
    base::TimeDelta start_timestamp) {
  // Don't start the timer if any of our state indicates we shouldn't; this
  // check is important since the various event handlers do not have to care
  // about the state of other events.
  if (!ShouldReportWatchTime() || !is_playing_ || !volume_ || !is_visible_) {
    // If we reach this point the timer should already have been stopped or
    // there is a pending finalize in flight.
    DCHECK(!reporting_timer_.IsRunning() || end_timestamp_ != kNoTimestamp);
    return;
  }

  // If we haven't finalized the last watch time metrics yet, count this
  // playback as a continuation of the previous metrics.
  if (end_timestamp_ != kNoTimestamp) {
    DCHECK(reporting_timer_.IsRunning());
    end_timestamp_ = kNoTimestamp;
    return;
  }

  // Don't restart the timer if it's already running.
  if (reporting_timer_.IsRunning())
    return;

  underflow_count_ = 0;
  last_media_timestamp_ = last_media_power_timestamp_ =
      last_media_controls_timestamp_ = end_timestamp_for_power_ =
          last_media_display_type_timestamp_ = end_timestamp_for_display_type_ =
              kNoTimestamp;
  is_on_battery_power_ = IsOnBatteryPower();
  display_type_for_recording_ = display_type_;
  start_timestamp_ = start_timestamp_for_power_ =
      start_timestamp_for_controls_ = start_timestamp_for_display_type_ =
          start_timestamp;
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::MaybeFinalizeWatchTime(FinalizeTime finalize_time) {
  // Don't finalize if the timer is already stopped.
  if (!reporting_timer_.IsRunning())
    return;

  // Don't trample an existing finalize; the first takes precedence.
  if (end_timestamp_ == kNoTimestamp)
    end_timestamp_ = get_media_time_cb_.Run();

  if (finalize_time == FinalizeTime::IMMEDIATELY) {
    UpdateWatchTime();
    return;
  }

  // Always restart the timer when finalizing, so that we allow for the full
  // length of |kReportingInterval| to elapse for hysteresis purposes.
  DCHECK_EQ(finalize_time, FinalizeTime::ON_NEXT_UPDATE);
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

void WatchTimeReporter::UpdateWatchTime() {
  DCHECK(ShouldReportWatchTime());

  const bool is_finalizing = end_timestamp_ != kNoTimestamp;
  const bool is_power_change_pending = end_timestamp_for_power_ != kNoTimestamp;
  const bool is_controls_change_pending =
      end_timestamp_for_controls_ != kNoTimestamp;
  const bool is_display_type_change_pending =
      end_timestamp_for_display_type_ != kNoTimestamp;

  // If we're finalizing the log, use the media time value at the time of
  // finalization.
  const base::TimeDelta current_timestamp =
      is_finalizing ? end_timestamp_ : get_media_time_cb_.Run();
  const base::TimeDelta elapsed = current_timestamp - start_timestamp_;

  std::unique_ptr<MediaLogEvent> log_event =
      media_log_->CreateEvent(MediaLogEvent::Type::WATCH_TIME_UPDATE);

#define RECORD_WATCH_TIME(key, value)                                      \
  do {                                                                     \
    log_event->params.SetDoubleWithoutPathExpansion(                       \
        has_video_ ? kWatchTimeAudioVideo##key                             \
                   : (is_background_ ? kWatchTimeAudioVideoBackground##key \
                                     : kWatchTimeAudio##key),              \
        value.InSecondsF());                                               \
  } while (0)

// Similar to RECORD_WATCH_TIME but ignores background watch time.
#define RECORD_FOREGROUND_WATCH_TIME(key, value)                       \
  do {                                                                 \
    DCHECK(!is_background_);                                           \
    log_event->params.SetDoubleWithoutPathExpansion(                   \
        has_video_ ? kWatchTimeAudioVideo##key : kWatchTimeAudio##key, \
        value.InSecondsF());                                           \
  } while (0)

  // Only report watch time after some minimum amount has elapsed. Don't update
  // watch time if media time hasn't changed since the last run; this may occur
  // if a seek is taking some time to complete or the playback is stalled for
  // some reason.
  if (last_media_timestamp_ != current_timestamp) {
    last_media_timestamp_ = current_timestamp;

    if (elapsed >= kMinimumElapsedWatchTime) {
      RECORD_WATCH_TIME(All, elapsed);
      if (is_mse_)
        RECORD_WATCH_TIME(Mse, elapsed);
      else
        RECORD_WATCH_TIME(Src, elapsed);

      if (is_encrypted_)
        RECORD_WATCH_TIME(Eme, elapsed);

      if (is_embedded_media_experience_enabled_)
        RECORD_WATCH_TIME(EmbeddedExperience, elapsed);
    }
  }

  if (last_media_power_timestamp_ != current_timestamp) {
    // We need a separate |last_media_power_timestamp_| since we don't always
    // base the last watch time calculation on the current timestamp.
    last_media_power_timestamp_ =
        is_power_change_pending ? end_timestamp_for_power_ : current_timestamp;

    // Record watch time using the last known value for |is_on_battery_power_|;
    // if there's a |pending_power_change_| use that to accurately finalize the
    // last bits of time in the previous bucket.
    const base::TimeDelta elapsed_power =
        last_media_power_timestamp_ - start_timestamp_for_power_;

    // Again, only update watch time if enough time has elapsed; we need to
    // recheck the elapsed time here since the power source can change anytime.
    if (elapsed_power >= kMinimumElapsedWatchTime) {
      if (is_on_battery_power_)
        RECORD_WATCH_TIME(Battery, elapsed_power);
      else
        RECORD_WATCH_TIME(Ac, elapsed_power);
    }
  }

  // Similar to the block above for controls.
  if (!is_background_ && last_media_controls_timestamp_ != current_timestamp) {
    last_media_controls_timestamp_ = is_controls_change_pending
                                         ? end_timestamp_for_controls_
                                         : current_timestamp;

    const base::TimeDelta elapsed_controls =
        last_media_controls_timestamp_ - start_timestamp_for_controls_;

    if (elapsed_controls >= kMinimumElapsedWatchTime) {
      if (has_native_controls_)
        RECORD_FOREGROUND_WATCH_TIME(NativeControlsOn, elapsed_controls);
      else
        RECORD_FOREGROUND_WATCH_TIME(NativeControlsOff, elapsed_controls);
    }
  }

  // Similar to the block above for display type.
  if (!is_background_ && has_video_ &&
      last_media_display_type_timestamp_ != current_timestamp) {
    last_media_display_type_timestamp_ = is_display_type_change_pending
                                             ? end_timestamp_for_display_type_
                                             : current_timestamp;

    const base::TimeDelta elapsed_display_type =
        last_media_display_type_timestamp_ - start_timestamp_for_display_type_;

    if (elapsed_display_type >= kMinimumElapsedWatchTime) {
      switch (display_type_for_recording_) {
        case blink::WebMediaPlayer::DisplayType::kInline:
          RECORD_FOREGROUND_WATCH_TIME(DisplayInline, elapsed_display_type);
          break;
        case blink::WebMediaPlayer::DisplayType::kFullscreen:
          RECORD_FOREGROUND_WATCH_TIME(DisplayFullscreen, elapsed_display_type);
          break;
        case blink::WebMediaPlayer::DisplayType::kPictureInPicture:
          RECORD_FOREGROUND_WATCH_TIME(DisplayPictureInPicture,
                                       elapsed_display_type);
          break;
      }
    }
  }

#undef RECORD_WATCH_TIME
#undef RECORD_FOREGROUND_WATCH_TIME

  // Pass along any underflow events which have occurred since the last report.
  if (!pending_underflow_events_.empty()) {
    if (!is_finalizing) {
      // The maximum value here per period is ~5 events, so int cast is okay.
      underflow_count_ += static_cast<int>(pending_underflow_events_.size());
    } else {
      // Only count underflow events prior to finalize.
      for (auto& ts : pending_underflow_events_) {
        if (ts <= end_timestamp_)
          underflow_count_++;
      }
    }

    log_event->params.SetInteger(kWatchTimeUnderflowCount, underflow_count_);
    pending_underflow_events_.clear();
  }

  // Always send finalize, even if we don't currently have any data, it's
  // harmless to send since nothing will be logged if we've already finalized.
  if (is_finalizing) {
    log_event->params.SetBoolean(kWatchTimeFinalize, true);
  } else {
    if (is_power_change_pending)
      log_event->params.SetBoolean(kWatchTimeFinalizePower, true);
    if (is_controls_change_pending)
      log_event->params.SetBoolean(kWatchTimeFinalizeControls, true);
    if (is_display_type_change_pending)
      log_event->params.SetBoolean(kWatchTimeFinalizeDisplay, true);
  }

  if (!log_event->params.empty())
    media_log_->AddEvent(std::move(log_event));

  if (is_power_change_pending) {
    // Invert battery power status here instead of using the value returned by
    // the PowerObserver since there may be a pending OnPowerStateChange().
    is_on_battery_power_ = !is_on_battery_power_;

    start_timestamp_for_power_ = end_timestamp_for_power_;
    end_timestamp_for_power_ = kNoTimestamp;
  }

  if (is_controls_change_pending) {
    has_native_controls_ = !has_native_controls_;

    start_timestamp_for_controls_ = end_timestamp_for_controls_;
    end_timestamp_for_controls_ = kNoTimestamp;
  }

  if (is_display_type_change_pending) {
    display_type_for_recording_ = display_type_;

    start_timestamp_for_display_type_ = end_timestamp_for_display_type_;
    end_timestamp_for_display_type_ = kNoTimestamp;
  }

  // Stop the timer if this is supposed to be our last tick.
  if (is_finalizing) {
    end_timestamp_ = kNoTimestamp;
    underflow_count_ = 0;
    reporting_timer_.Stop();
  }
}

void WatchTimeReporter::OnDisplayTypeChanged(
    blink::WebMediaPlayer::DisplayType display_type) {
  display_type_ = display_type;

  if (!reporting_timer_.IsRunning())
    return;

  if (display_type_for_recording_ == display_type_) {
    end_timestamp_for_display_type_ = kNoTimestamp;
    return;
  }

  end_timestamp_for_display_type_ = get_media_time_cb_.Run();
  reporting_timer_.Start(FROM_HERE, reporting_interval_, this,
                         &WatchTimeReporter::UpdateWatchTime);
}

}  // namespace media
