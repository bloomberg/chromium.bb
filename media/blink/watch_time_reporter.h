// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WATCH_TIME_REPORTER_H_
#define MEDIA_BLINK_WATCH_TIME_REPORTER_H_

#include <vector>

#include "base/callback.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/media_log.h"
#include "media/base/timestamp_constants.h"
#include "media/blink/media_blink_export.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Class for monitoring and reporting watch time in response to various state
// changes during the playback of media. We record metrics for audio only
// playbacks as well as audio+video playbacks of sufficient size.
//
// Watch time for our purposes is defined as the amount of elapsed media time
// for audio only or audio+video media. A minimum of 7 seconds of unmuted media
// must be watched to start watch time monitoring. Watch time is checked every 5
// seconds from then on and reported to multiple buckets: All, MSE, SRC, EME,
// AC, and battery.
//
// Any one of paused, hidden (where this is video), or muted is sufficient to
// stop watch time metric reports. Each of these has a hysteresis where if the
// state change is undone within 5 seconds, the watch time will be counted as
// uninterrupted.
//
// If the media is audio+video, foreground watch time is logged to the normal
// AudioVideo bucket, while background watch time goes to the specific
// AudioVideo.Background bucket. As with other events, there is hysteresis on
// change between the foreground and background.
//
// Power events (on/off battery power) have a similar hysteresis, but unlike
// the aforementioned properties, will not stop metric collection.
//
// Each seek event will result in a new watch time metric being started and the
// old metric finalized as accurately as possible.
class MEDIA_BLINK_EXPORT WatchTimeReporter : base::PowerObserver {
 public:
  using GetMediaTimeCB = base::Callback<base::TimeDelta(void)>;

  // Constructor for the reporter; all requested metadata should be fully known
  // before attempting construction as incorrect values will result in the wrong
  // watch time metrics being reported.
  //
  // |media_log| is used to continuously log the watch time values for eventual
  // recording to a histogram upon finalization.
  //
  // |initial_video_size| required to ensure that the video track has sufficient
  // size for watch time reporting.
  //
  // |get_media_time_cb| must return the current playback time in terms of media
  // time, not wall clock time! Using media time instead of wall clock time
  // allows us to avoid a whole class of issues around clock changes during
  // suspend and resume.
  // TODO(dalecurtis): Should we only report when rate == 1.0? Should we scale
  // the elapsed media time instead?
  WatchTimeReporter(bool has_audio,
                    bool has_video,
                    bool is_mse,
                    bool is_encrypted,
                    bool is_embedded_media_experience_enabled,
                    MediaLog* media_log,
                    const gfx::Size& initial_video_size,
                    const GetMediaTimeCB& get_media_time_cb);
  ~WatchTimeReporter() override;

  // These methods are used to ensure that watch time is only reported for
  // media that is actually playing. They should be called whenever the media
  // starts or stops playing for any reason. If the media is audio+video and
  // currently hidden, OnPlaying() will start background watch time reporting.
  void OnPlaying();
  void OnPaused();

  // This will immediately finalize any outstanding watch time reports and stop
  // the reporting timer. Clients should call OnPlaying() upon seek completion
  // to restart the reporting timer.
  void OnSeeking();

  // This method is used to ensure that watch time is only reported for media
  // that is actually audible to the user. It should be called whenever the
  // volume changes.
  //
  // Note: This does not catch all cases. E.g., headphones that are being
  // listened too, or even OS level volume state.
  void OnVolumeChange(double volume);

  // These methods are used to ensure that watch time is only reported for
  // videos that are actually visible to the user. They should be called when
  // the video is shown or hidden respectively. OnHidden() will start background
  // watch time reporting if the media is audio+video.
  //
  // TODO(dalecurtis): At present, this is only called when the entire content
  // window goes into the foreground or background respectively; i.e. it does
  // not catch cases where the video is in the foreground but out of the view
  // port. We need a method for rejecting out of view port videos.
  void OnShown();
  void OnHidden();

  // Returns true if the current size is large enough that watch time will be
  // recorded for playback.
  bool IsSizeLargeEnoughToReportWatchTime() const;

  // Indicates a rebuffering event occurred during playback. When watch time is
  // finalized the total watch time for a given category will be divided by the
  // number of rebuffering events. Reset to zero after a finalize event.
  void OnUnderflow();

  // These methods are used to ensure that the watch time is reported relative
  // to whether the media is using native controls.
  void OnNativeControlsEnabled();
  void OnNativeControlsDisabled();

  // These methods are used to ensure that the watch time is reported relative
  // to the display type of the media.
  void OnDisplayTypeInline();
  void OnDisplayTypeFullscreen();
  void OnDisplayTypePictureInPicture();

  // Setup the reporting interval to be immediate to avoid spinning real time
  // within the unit test.
  void set_reporting_interval_for_testing() {
    reporting_interval_ = base::TimeDelta();
  }

  void set_is_on_battery_power_for_testing(bool on_battery_power) {
    is_on_battery_power_ = on_battery_power;
  }

  void OnPowerStateChangeForTesting(bool on_battery_power) {
    OnPowerStateChange(on_battery_power);
  }

 private:
  friend class WatchTimeReporterTest;

  // Internal constructor for marking background status.
  WatchTimeReporter(bool has_audio,
                    bool has_video,
                    bool is_mse,
                    bool is_encrypted,
                    bool is_embedded_media_experience_enabled,
                    MediaLog* media_log,
                    const gfx::Size& initial_video_size,
                    const GetMediaTimeCB& get_media_time_cb,
                    bool is_background);

  // base::PowerObserver implementation.
  //
  // We only observe power source changes. We don't need to observe suspend and
  // resume events because we report watch time in terms of elapsed media time
  // and not in terms of elapsed real time.
  void OnPowerStateChange(bool on_battery_power) override;

  bool ShouldReportWatchTime();
  void MaybeStartReportingTimer(base::TimeDelta start_timestamp);
  enum class FinalizeTime { IMMEDIATELY, ON_NEXT_UPDATE };
  void MaybeFinalizeWatchTime(FinalizeTime finalize_time);
  void UpdateWatchTime();
  void OnDisplayTypeChanged(blink::WebMediaPlayer::DisplayType display_type);

  // Initialized during construction.
  const bool has_audio_;
  const bool has_video_;
  const bool is_mse_;
  const bool is_encrypted_;
  const bool is_embedded_media_experience_enabled_;
  MediaLog* media_log_;
  const gfx::Size initial_video_size_;
  const GetMediaTimeCB get_media_time_cb_;
  const bool is_background_;

  // The amount of time between each UpdateWatchTime(); this is the frequency by
  // which the watch times are updated. In the event of a process crash or kill
  // this is also the most amount of watch time that we might lose.
  base::TimeDelta reporting_interval_ = base::TimeDelta::FromSeconds(5);

  base::RepeatingTimer reporting_timer_;

  // Updated by the OnXXX() methods above.
  bool is_on_battery_power_ = false;
  bool is_playing_ = false;
  bool is_visible_ = true;
  bool has_native_controls_ = false;
  double volume_ = 1.0;
  int underflow_count_ = 0;
  std::vector<base::TimeDelta> pending_underflow_events_;
  blink::WebMediaPlayer::DisplayType display_type_ =
      blink::WebMediaPlayer::DisplayType::kInline;
  blink::WebMediaPlayer::DisplayType display_type_for_recording_ =
      blink::WebMediaPlayer::DisplayType::kInline;

  // The last media timestamp seen by UpdateWatchTime().
  base::TimeDelta last_media_timestamp_ = kNoTimestamp;
  base::TimeDelta last_media_power_timestamp_ = kNoTimestamp;
  base::TimeDelta last_media_controls_timestamp_ = kNoTimestamp;
  base::TimeDelta last_media_display_type_timestamp_ = kNoTimestamp;

  // The starting and ending timestamps used for reporting watch time.
  base::TimeDelta start_timestamp_;
  base::TimeDelta end_timestamp_ = kNoTimestamp;

  // Similar to the above but tracks watch time relative to whether or not
  // battery or AC power is being used.
  base::TimeDelta start_timestamp_for_power_;
  base::TimeDelta end_timestamp_for_power_ = kNoTimestamp;

  // Similar to the above but tracks watch time relative to whether or not
  // native controls are being used.
  base::TimeDelta start_timestamp_for_controls_;
  base::TimeDelta end_timestamp_for_controls_ = kNoTimestamp;

  // Similar to the above but tracks watch time relative to whether the display
  // type is inline, fullscreen or picture-in-picture.
  base::TimeDelta start_timestamp_for_display_type_;
  base::TimeDelta end_timestamp_for_display_type_ = kNoTimestamp;

  // Special case reporter for handling background video watch time. Configured
  // as an audio only WatchTimeReporter with |is_background_| set to true.
  std::unique_ptr<WatchTimeReporter> background_reporter_;

  DISALLOW_COPY_AND_ASSIGN(WatchTimeReporter);
};

}  // namespace media

#endif  // MEDIA_BLINK_WATCH_TIME_REPORTER_H_
