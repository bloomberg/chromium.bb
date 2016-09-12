// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_engagement/desktop_engagement_service.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "components/variations/variations_associated_data.h"

namespace metrics {

namespace {

DesktopEngagementService* g_instance = nullptr;

}  // namespace

// static
void DesktopEngagementService::Initialize() {
  g_instance = new DesktopEngagementService;
}

// static
bool DesktopEngagementService::IsInitialized() {
  return g_instance != nullptr;
}

// static
DesktopEngagementService* DesktopEngagementService::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void DesktopEngagementService::StartTimer(base::TimeDelta duration) {
  timer_.Start(FROM_HERE, duration,
               base::Bind(&DesktopEngagementService::OnTimerFired,
                          weak_factory_.GetWeakPtr()));
}

void DesktopEngagementService::OnVisibilityChanged(bool visible) {
  is_visible_ = visible;
  if (is_visible_ && !is_first_session_) {
    OnUserEvent();
  } else if (in_session_ && !is_audio_playing_) {
    DVLOG(4) << "Ending session due to visibility change";
    EndSession();
  }
}

void DesktopEngagementService::OnUserEvent() {
  if (!is_visible_)
    return;

  last_user_event_ = base::TimeTicks::Now();
  // This may start session.
  if (!in_session_) {
    DVLOG(4) << "Starting session due to user event";
    StartSession();
  }
}

void DesktopEngagementService::OnAudioStart() {
  // This may start session.
  is_audio_playing_ = true;
  if (!in_session_) {
    DVLOG(4) << "Starting session due to audio start";
    StartSession();
  }
}

void DesktopEngagementService::OnAudioEnd() {
  is_audio_playing_ = false;

  // If the timer is not running, this means that no user events happened in the
  // last 5 minutes so the session can be terminated.
  if (!timer_.IsRunning()) {
    DVLOG(4) << "Ending session due to audio ending";
    EndSession();
  }
}

DesktopEngagementService::DesktopEngagementService()
    : session_start_(base::TimeTicks::Now()),
      last_user_event_(session_start_),
      audio_tracker_(this),
      weak_factory_(this) {
  InitInactivityTimeout();
}

DesktopEngagementService::~DesktopEngagementService() {}

void DesktopEngagementService::OnTimerFired() {
  base::TimeDelta remaining =
      inactivity_timeout_ - (base::TimeTicks::Now() - last_user_event_);
  if (remaining.ToInternalValue() > 0) {
    StartTimer(remaining);
    return;
  }

  // No user events happened in the last 5 min. Terminate the session now.
  if (!is_audio_playing_) {
    DVLOG(4) << "Ending session after delay";
    EndSession();
  }
}

void DesktopEngagementService::StartSession() {
  in_session_ = true;
  is_first_session_ = false;
  session_start_ = base::TimeTicks::Now();
  StartTimer(inactivity_timeout_);
}

void DesktopEngagementService::EndSession() {
  in_session_ = false;

  base::TimeDelta delta = base::TimeTicks::Now() - session_start_;
  DVLOG(4) << "Logging session length of " << delta.InSeconds() << " seconds.";

  // Note: This metric is recorded separately for Android in
  // UmaSessionStats::UmaEndSession.
  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", delta);
}

void DesktopEngagementService::InitInactivityTimeout() {
  const int kDefaultInactivityTimeoutMinutes = 5;

  int timeout_minutes = kDefaultInactivityTimeoutMinutes;
  std::string param_value = variations::GetVariationParamValue(
      "DesktopEngagement", "inactivity_timeout");
  if (!param_value.empty())
    base::StringToInt(param_value, &timeout_minutes);

  inactivity_timeout_ = base::TimeDelta::FromMinutes(timeout_minutes);
}

}  // namespace metrics
