// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace vr_shell {

enum class VRMode {
  NO_VR = 0,
  VR_BROWSER = 1,     // VR Shell.
  VR_FULLSCREEN = 2,  // Cinema mode.
  WEBVR = 3,
};

// SessionTimer will monitor the time between calls to StartSession and
// StopSession.  It will combine multiple segments into a single session if they
// are sufficiently close in time.  It will also only include segments if they
// are sufficiently long.
// Because the session may be extended, the accumulated time is occasionally
// sent on destruction or when a new session begins.
class SessionTimer {
 public:
  virtual ~SessionTimer() {}
  void StartSession(base::Time startTime);
  void StopSession(bool continuable, base::Time stopTime);

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

// Most methods of this class are not threadsafe - they must be called
// on the Browser's main thread.  This happens by default except for
// SetWebVREnabled and SetVRActive, where we'll post a message to get to the
// correct thread.
// Destruction must happen on the main thread, or there could be corruption when
// WebContentsObserver unregisters itself from the web_contents_.
// There are DCHECKS to ensure these threading rules are satisfied.
class VrMetricsHelper : public content::WebContentsObserver,
                        public base::RefCountedThreadSafe<VrMetricsHelper> {
 public:
  explicit VrMetricsHelper(content::WebContents*);

  void SetWebVREnabled(bool is_webvr_presenting);
  void SetVRActive(bool is_vr_enabled);

  // WebContentObserver
  void MediaStartedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId&) override;
  void MediaStoppedPlaying(const MediaPlayerInfo& media_info,
                           const MediaPlayerId&) override;
  void DidFinishNavigation(content::NavigationHandle*) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;

 protected:
  friend class base::RefCountedThreadSafe<VrMetricsHelper>;
  ~VrMetricsHelper() override;

  void SetWebVREnabledOnMainThread(bool is_webvr_presenting);
  void SetVRActiveOnMainThread(bool is_vr_enabled);

  void SetVrMode(VRMode mode);
  void UpdateMode();

  std::unique_ptr<SessionTimer> mode_video_timer_;
  std::unique_ptr<SessionTimer> session_video_timer_;
  std::unique_ptr<SessionTimer> mode_timer_;
  std::unique_ptr<SessionTimer> session_timer_;

  VRMode mode_ = VRMode::NO_VR;

  // state that gets translated into vr_mode:
  bool is_fullscreen_ = false;
  bool is_webvr_ = false;
  bool is_vr_enabled_ = false;

  int num_videos_playing_ = 0;
  int num_session_navigation_ = 0;
  int num_session_video_playback_ = 0;

  GURL origin_;

  // not thread safe, so ensure we are on the same thread at all times:
  base::PlatformThreadId thread_id_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_USAGE_MONITOR_H_
