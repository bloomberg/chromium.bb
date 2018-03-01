// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_VR_USAGE_MONITOR_H_
#define CHROME_BROWSER_ANDROID_VR_VR_USAGE_MONITOR_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/vr/mode.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace vr {

// SessionTimer will monitor the time between calls to StartSession and
// StopSession.  It will combine multiple segments into a single session if they
// are sufficiently close in time.  It will also only include segments if they
// are sufficiently long.
// Because the session may be extended, the accumulated time is occasionally
// sent on destruction or when a new session begins.
class SessionTimer {
 public:
  virtual ~SessionTimer() {}

  void StartSession(base::Time start_time);
  void StopSession(bool continuable, base::Time stop_time);

 protected:
  SessionTimer() {}

  virtual void SendAccumulatedSessionTime() = 0;

  base::Time start_time_;
  base::Time stop_time_;
  base::TimeDelta accumulated_time_;

  // config members:
  // time between stop and start to count as same session
  base::TimeDelta maximum_session_gap_time_;

  // minimum time between start and stop to add to duration
  base::TimeDelta minimum_duration_;

  DISALLOW_COPY_AND_ASSIGN(SessionTimer);
};

// SessionTracker tracks UKM data for sessions and sends the data upon request.
template <class T>
class SessionTracker {
 public:
  explicit SessionTracker(std::unique_ptr<T> entry)
      : ukm_entry_(std::move(entry)),
        start_time_(base::Time::Now()),
        stop_time_(base::Time::Now()) {}
  virtual ~SessionTracker() {}
  T* ukm_entry() { return ukm_entry_.get(); }
  void SetSessionEnd(base::Time stop_time) { stop_time_ = stop_time; }

  int GetRoundedDurationInSeconds() {
    if (start_time_ > stop_time_) {
      // Return negative one to indicate an invalid value was recorded.
      return -1;
    }

    base::TimeDelta duration = stop_time_ - start_time_;

    if (duration.InHours() > 1) {
      return duration.InHours() * 3600;
    } else if (duration.InMinutes() > 10) {
      return (duration.InMinutes() / 10) * 10 * 60;
    } else if (duration.InSeconds() > 60) {
      return duration.InMinutes() * 60;
    } else {
      return duration.InSeconds();
    }
  }

  void RecordEntry() {
    ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
    DCHECK(ukm_recorder);

    ukm_entry_->Record(ukm_recorder);
  }

 protected:
  std::unique_ptr<T> ukm_entry_;

  base::Time start_time_;
  base::Time stop_time_;

  DISALLOW_COPY_AND_ASSIGN(SessionTracker);
};

// This class is not thread-safe and must only be used from the main thread.
class VrMetricsHelper : public content::WebContentsObserver {
 public:
  VrMetricsHelper(content::WebContents* contents,
                  Mode initial_mode,
                  bool started_with_autopresentation);
  ~VrMetricsHelper() override;

  void SetWebVREnabled(bool is_webvr_presenting);
  void SetVRActive(bool is_vr_enabled);
  void RecordVoiceSearchStarted();
  void RecordUrlRequestedByVoice(GURL url);

 private:
  // WebContentObserver
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId&) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const MediaPlayerId&,
      WebContentsObserver::MediaStoppedReason reason) override;
  void DidStartNavigation(content::NavigationHandle* handle) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

  void SetVrMode(Mode mode);
  void UpdateMode();

  std::unique_ptr<SessionTimer> mode_video_timer_;
  std::unique_ptr<SessionTimer> session_video_timer_;
  std::unique_ptr<SessionTimer> mode_timer_;
  std::unique_ptr<SessionTimer> session_timer_;

  std::unique_ptr<SessionTracker<ukm::builders::XR_PageSession>>
      page_session_tracker_;
  std::unique_ptr<SessionTracker<ukm::builders::XR_WebXR_PresentationSession>>
      presentation_session_tracker_;

  Mode mode_ = Mode::kNoVr;

  // state that gets translated into vr_mode:
  bool is_fullscreen_ = false;
  bool is_webvr_ = false;
  bool is_vr_enabled_ = false;
  bool started_with_autopresentation_ = false;

  GURL url_requested_by_voice_;

  int num_videos_playing_ = 0;
  int num_session_navigation_ = 0;
  int num_session_video_playback_ = 0;
  int num_voice_search_started_ = 0;

  GURL origin_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_ANDROID_VR_VR_USAGE_MONITOR_H_
