// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_usage_monitor.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/rappor/public/rappor_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace vr_shell {

namespace {

// minimum duration: 7 seconds for video, no minimum for headset/vr modes
// maximum gap: 7 seconds between videos.  no gap for headset/vr-modes
constexpr base::TimeDelta kMinimumVideoSessionDuration(
    base::TimeDelta::FromSecondsD(7));
constexpr base::TimeDelta kMaximumVideoSessionGap(
    base::TimeDelta::FromSecondsD(7));

constexpr base::TimeDelta kMinimumHeadsetSessionDuration(
    base::TimeDelta::FromSecondsD(0));
constexpr base::TimeDelta kMaximumHeadsetSessionGap(
    base::TimeDelta::FromSecondsD(0));

// We have several different session times that share code in SessionTimer.
// Unfortunately, when the actual timer histogram is processed in
// UMA_HISTOGRAM_CUSTOM_TIMES, there is a function-static variable initialized
// with the name of the event, and all histograms going through the same
// function must share the same event name.
// In order to work around this and use different names, a templated function
// is used to get different function-static variables for each histogram name.
// Ideally this could be templated by the event name, but unfortunately
// C++ doesn't allow templates by strings.  Instead we template by enum, and
// have a function that translates enum to string.  For each template
// instantiation, the inlined function will be optimized to just access the
// string we want to return.
enum SessionEventName {
  MODE_FULLSCREEN,
  MODE_BROWSER,
  MODE_WEBVR,
  SESSION_VR,
  MODE_FULLSCREEN_WITH_VIDEO,
  MODE_BROWSER_WITH_VIDEO,
  MODE_WEBVR_WITH_VIDEO,
  SESSION_VR_WITH_VIDEO,
};

const char* HistogramNameFromSessionType(SessionEventName name) {
  static constexpr char kVrSession[] = "VRSessionTime";
  static constexpr char kWebVr[] = "VRSessionTime.WebVR";
  static constexpr char kBrowser[] = "VRSessionTime.Browser";
  static constexpr char kFullscreen[] = "VRSessionTime.Fullscreen";
  static constexpr char kVrSessionVideo[] = "VRSessionVideTime";
  static constexpr char kWebVrVideo[] = "VRSessionVideoTime.WebVR";
  static constexpr char kBrowserVideo[] = "VRSessionVideoTime.Browser";
  static constexpr char kFullscreenVideo[] = "VRSessionVideoTime.Fullscreen";

  switch (name) {
    case MODE_FULLSCREEN:
      return kFullscreen;
    case MODE_BROWSER:
      return kBrowser;
    case MODE_WEBVR:
      return kWebVr;
    case SESSION_VR:
      return kVrSession;
    case MODE_FULLSCREEN_WITH_VIDEO:
      return kFullscreenVideo;
    case MODE_BROWSER_WITH_VIDEO:
      return kBrowserVideo;
    case MODE_WEBVR_WITH_VIDEO:
      return kWebVrVideo;
    case SESSION_VR_WITH_VIDEO:
      return kVrSessionVideo;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void SendRapporEnteredMode(const GURL& origin, VRMode mode) {
  switch (mode) {
    case VRMode::VR_FULLSCREEN:
      rappor::SampleDomainAndRegistryFromGURL(rappor::GetDefaultService(),
                                              "VR.FullScreenMode", origin);
    default:
      break;
  }
}

void SendRapporEnteredVideoMode(const GURL& origin, VRMode mode) {
  switch (mode) {
    case VRMode::VR_BROWSER:
      rappor::SampleDomainAndRegistryFromGURL(rappor::GetDefaultService(),
                                              "VR.Video.Browser", origin);
    case VRMode::WEBVR:
      rappor::SampleDomainAndRegistryFromGURL(rappor::GetDefaultService(),
                                              "VR.Video.WebVR", origin);
    case VRMode::VR_FULLSCREEN:
      rappor::SampleDomainAndRegistryFromGURL(
          rappor::GetDefaultService(), "VR.Video.FullScreenMode", origin);
    default:
      break;
  }
}
}  // namespace

template <SessionEventName SessionType>
class SessionTimerImpl : public SessionTimer {
 public:
  SessionTimerImpl(base::TimeDelta gap_time, base::TimeDelta minimum_duration) {
    maximum_session_gap_time_ = gap_time;
    minimum_duration_ = minimum_duration;
  }

  ~SessionTimerImpl() override { StopSession(false, base::Time::Now()); }

  void SendAccumulatedSessionTime() override {
    if (!accumulated_time_.is_zero()) {
      UMA_HISTOGRAM_CUSTOM_TIMES(HistogramNameFromSessionType(SessionType),
                                 accumulated_time_, base::TimeDelta(),
                                 base::TimeDelta::FromHours(5), 100);
    }
  }
};

void SessionTimer::StartSession(base::Time start_time) {
  // If the new start time is within the minimum session gap time from the last
  // stop, continue the previous session.
  // Otherwise, start a new session, sending the event for the last session.
  if (!stop_time_.is_null() &&
      start_time - stop_time_ <= maximum_session_gap_time_) {
    // Mark the previous segment as non-continuable, sending data and clearing
    // state.
    StopSession(false, stop_time_);
  }

  start_time_ = start_time;
}

void SessionTimer::StopSession(bool continuable, base::Time stop_time) {
  // first accumulate time from this segment of the session
  base::TimeDelta segment_duration =
      (start_time_.is_null() ? base::TimeDelta() : stop_time - start_time_);
  if (!segment_duration.is_zero() && segment_duration > minimum_duration_) {
    accumulated_time_ = accumulated_time_ + segment_duration;
  }

  if (continuable) {
    // if we are continuable, accumulate the current segment to the session, and
    // set stop_time_ so we may continue later
    accumulated_time_ = stop_time - start_time_ + accumulated_time_;
    stop_time_ = stop_time;
    start_time_ = base::Time();
  } else {
    // send the histogram now if we aren't continuable, clearing segment state
    SendAccumulatedSessionTime();

    // clear out start/stop/accumulated time
    start_time_ = base::Time();
    stop_time_ = base::Time();
    accumulated_time_ = base::TimeDelta();
  }
}

void VrMetricsHelper::UpdateMode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VRMode mode;
  if (!is_vr_enabled_) {
    mode = VRMode::NO_VR;
  } else if (is_webvr_) {
    mode = VRMode::WEBVR;
  } else {
    mode = is_fullscreen_ ? VRMode::VR_FULLSCREEN : VRMode::VR_BROWSER;
  }

  if (mode != mode_)
    SetVrMode(mode);
}

void VrMetricsHelper::SetWebVREnabled(bool is_webvr_presenting) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_webvr_ = is_webvr_presenting;
  UpdateMode();
}

void VrMetricsHelper::SetVRActive(bool is_vr_enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_vr_enabled_ = is_vr_enabled;
  UpdateMode();
}

void VrMetricsHelper::SetVrMode(VRMode mode) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(mode, mode_);

  base::Time switch_time = base::Time::Now();

  // stop the previous modes
  if (mode_ != VRMode::NO_VR) {
    if (num_videos_playing_ > 0)
      mode_video_timer_->StopSession(false, switch_time);

    mode_timer_->StopSession(false, switch_time);
  }

  // start the new modes
  if (mode != VRMode::NO_VR) {
    switch (mode) {
      case VRMode::WEBVR:
        mode_timer_ = base::MakeUnique<SessionTimerImpl<MODE_WEBVR>>(
            kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        mode_video_timer_ =
            base::MakeUnique<SessionTimerImpl<MODE_WEBVR_WITH_VIDEO>>(
                kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        break;
      case VRMode::VR_BROWSER:
        mode_timer_ = base::MakeUnique<SessionTimerImpl<MODE_BROWSER>>(
            kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        mode_video_timer_ =
            base::MakeUnique<SessionTimerImpl<MODE_BROWSER_WITH_VIDEO>>(
                kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        break;
      case VRMode::VR_FULLSCREEN:
        mode_timer_ = base::MakeUnique<SessionTimerImpl<MODE_FULLSCREEN>>(
            kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        mode_video_timer_ =
            base::MakeUnique<SessionTimerImpl<MODE_FULLSCREEN_WITH_VIDEO>>(
                kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
        break;
      default:
        NOTREACHED();
    }

    mode_timer_->StartSession(switch_time);
    if (num_videos_playing_ > 0) {
      mode_video_timer_->StartSession(switch_time);
      SendRapporEnteredVideoMode(origin_, mode);
    }

    SendRapporEnteredMode(origin_, mode);
  }

  // stop the old session
  if (mode_ != VRMode::NO_VR && mode == VRMode::NO_VR) {
    if (num_videos_playing_ > 0)
      session_video_timer_->StopSession(false, switch_time);

    session_timer_->StopSession(false, switch_time);

    UMA_HISTOGRAM_COUNTS_100("VRSessionVideoCount",
                             num_session_video_playback_);
    UMA_HISTOGRAM_COUNTS_100("VRSessionNavigationCount",
                             num_session_navigation_);
  }

  // start the new session
  if (mode_ == VRMode::NO_VR && mode != VRMode::NO_VR) {
    // we are entering a vr mode from non-vr mode - start the vr session
    session_timer_->StartSession(switch_time);
    num_session_video_playback_ = 0;
    num_session_navigation_ = 0;

    if (num_videos_playing_ > 0) {
      session_video_timer_->StartSession(switch_time);
      num_session_video_playback_ = num_videos_playing_;
    }
  }

  mode_ = mode;
}

VrMetricsHelper::VrMetricsHelper(content::WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  num_videos_playing_ = contents->GetCurrentlyPlayingVideoCount();
  is_fullscreen_ = contents->IsFullscreen();
  origin_ = contents->GetLastCommittedURL();

  Observe(contents);
  session_timer_ = base::MakeUnique<SessionTimerImpl<SESSION_VR>>(
      kMaximumHeadsetSessionGap, kMinimumHeadsetSessionDuration);
  session_video_timer_ =
      base::MakeUnique<SessionTimerImpl<SESSION_VR_WITH_VIDEO>>(
          kMaximumVideoSessionGap, kMinimumVideoSessionDuration);
}

VrMetricsHelper::~VrMetricsHelper() = default;

void VrMetricsHelper::MediaStartedPlaying(const MediaPlayerInfo& media_info,
                                          const MediaPlayerId&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  if (num_videos_playing_ == 0) {
    // started playing video - start sessions
    base::Time start_time = base::Time::Now();

    if (mode_ != VRMode::NO_VR) {
      session_video_timer_->StartSession(start_time);
      mode_video_timer_->StartSession(start_time);
      SendRapporEnteredVideoMode(origin_, mode_);
    }
  }

  num_videos_playing_++;
  num_session_video_playback_++;
}

void VrMetricsHelper::MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                                          const MediaPlayerId&) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!media_info.has_video)
    return;

  num_videos_playing_--;

  if (num_videos_playing_ == 0) {
    // stopped playing video - update existing video sessions
    base::Time stop_time = base::Time::Now();

    if (mode_ != VRMode::NO_VR) {
      session_video_timer_->StopSession(true, stop_time);
      mode_video_timer_->StopSession(true, stop_time);
    }
  }
}

void VrMetricsHelper::DidFinishNavigation(content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (handle && handle->HasCommitted() && handle->IsInMainFrame()) {
    origin_ = handle->GetURL();
    // Counting the number of pages viewed is difficult - some websites load
    // new content dynamically without a navigation.  Others redirect several
    // times for a single navigation.
    // We look at the number of committed navigations in the main frame, which
    // will slightly overestimate pages viewed instead of trying to filter or
    // look at page loads, since those will underestimate on some pages, and
    // overestimate on others.
    num_session_navigation_++;
  }
}

void VrMetricsHelper::DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                                    bool will_cause_resize) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_fullscreen_ = entered_fullscreen;
  UpdateMode();
}

}  // namespace vr_shell
