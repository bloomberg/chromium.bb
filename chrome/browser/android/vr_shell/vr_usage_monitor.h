// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_

#include <memory>

#include "base/time/time.h"
#include "chrome/browser/vr/mode.h"
#include "content/public/browser/web_contents_observer.h"

namespace vr_shell {

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

// This class is not thread-safe and must only be used from the main thread.
class VrMetricsHelper : public content::WebContentsObserver {
 public:
  VrMetricsHelper(content::WebContents* contents,
                  vr::Mode initial_mode,
                  bool started_with_autopresentation);
  ~VrMetricsHelper() override;

  void SetWebVREnabled(bool is_webvr_presenting);
  void SetVRActive(bool is_vr_enabled);
  void RecordVoiceSearchStarted();

 private:
  // WebContentObserver
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId&) override;
  void MediaStoppedPlaying(
      const MediaPlayerInfo& media_info,
      const MediaPlayerId&,
      WebContentsObserver::MediaStoppedReason reason) override;
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

  void SetVrMode(vr::Mode mode);
  void UpdateMode();

  std::unique_ptr<SessionTimer> mode_video_timer_;
  std::unique_ptr<SessionTimer> session_video_timer_;
  std::unique_ptr<SessionTimer> mode_timer_;
  std::unique_ptr<SessionTimer> session_timer_;

  vr::Mode mode_ = vr::Mode::kNoVr;

  // state that gets translated into vr_mode:
  bool is_fullscreen_ = false;
  bool is_webvr_ = false;
  bool is_vr_enabled_ = false;
  bool started_with_autopresentation_ = false;

  int num_videos_playing_ = 0;
  int num_session_navigation_ = 0;
  int num_session_video_playback_ = 0;
  int num_voice_search_started_ = 0;

  GURL origin_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_
