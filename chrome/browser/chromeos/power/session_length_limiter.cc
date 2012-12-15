// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/session_length_limiter.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/prefs/public/pref_service_base.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

namespace {

// The minimum session time limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000; // 30 seconds.

// The maximum session time limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000; // 24 hours.

// The interval at which to fire periodic callbacks and check whether the
// session time limit has been reached.
const int kSessionLengthLimitTimerIntervalMs = 1000;

// A default delegate implementation that returns the current time and does end
// the current user's session when requested. This can be replaced with a mock
// in tests.
class SessionLengthLimiterDelegateImpl : public SessionLengthLimiter::Delegate {
 public:
  SessionLengthLimiterDelegateImpl();
  virtual ~SessionLengthLimiterDelegateImpl();

  virtual const base::Time GetCurrentTime() const;
  virtual void StopSession();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionLengthLimiterDelegateImpl);
};

SessionLengthLimiterDelegateImpl::SessionLengthLimiterDelegateImpl() {
}

SessionLengthLimiterDelegateImpl::~SessionLengthLimiterDelegateImpl() {
}

const base::Time SessionLengthLimiterDelegateImpl::GetCurrentTime() const {
  return base::Time::Now();
}

void SessionLengthLimiterDelegateImpl::StopSession() {
  browser::AttemptUserExit();
}

}  // namespace

SessionLengthLimiter::Delegate::~Delegate() {
}

// static
void SessionLengthLimiter::RegisterPrefs(PrefService* local_state) {
  local_state->RegisterInt64Pref(prefs::kSessionStartTime,
                                 0,
                                 PrefService::UNSYNCABLE_PREF);
  local_state->RegisterIntegerPref(prefs::kSessionLengthLimit,
                                   0,
                                   PrefService::UNSYNCABLE_PREF);
}

SessionLengthLimiter::SessionLengthLimiter(Delegate* delegate,
                                           bool browser_restarted)
    : delegate_(delegate ? delegate : new SessionLengthLimiterDelegateImpl) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If this is a user login, set the session start time in local state to the
  // current time. If this a browser restart after a crash, set the session
  // start time only if its current value appears corrupted (value unset, value
  // lying in the future, zero value).
  PrefService* local_state = g_browser_process->local_state();
  int64 session_start_time = local_state->GetInt64(prefs::kSessionStartTime);
  int64 now = delegate_->GetCurrentTime().ToInternalValue();
  if (!browser_restarted ||
      session_start_time <= 0 || session_start_time > now) {
    local_state->SetInt64(prefs::kSessionStartTime, now);
    // Ensure that the session start time is persisted to local state.
    local_state->CommitPendingWrite();
    session_start_time = now;
  }
  session_start_time_ = base::Time::FromInternalValue(session_start_time);

  // Listen for changes to the session length limit.
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kSessionLengthLimit,
      base::Bind(&SessionLengthLimiter::OnSessionLengthLimitChanged,
                 base::Unretained(this)));
  OnSessionLengthLimitChanged();
}

void SessionLengthLimiter::OnSessionLengthLimitChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  int limit;
  const PrefServiceBase::Preference* session_length_limit_pref =
      pref_change_registrar_.prefs()->
          FindPreference(prefs::kSessionLengthLimit);
  // If no session length limit is set, stop the timer.
  if (session_length_limit_pref->IsDefaultValue() ||
      !session_length_limit_pref->GetValue()->GetAsInteger(&limit)) {
    session_length_limit_ = base::TimeDelta();
    StopTimer();
    return;
  }

  // If a session length limit is set, clamp it to the valid range and start
  // the timer.
  session_length_limit_ = base::TimeDelta::FromMilliseconds(
      std::min(std::max(limit, kSessionLengthLimitMinMs),
               kSessionLengthLimitMaxMs));
  StartTimer();
}

void SessionLengthLimiter::StartTimer() {
  if (repeating_timer_ && repeating_timer_->IsRunning())
    return;
  if (!repeating_timer_)
    repeating_timer_.reset(new base::RepeatingTimer<SessionLengthLimiter>);
  repeating_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kSessionLengthLimitTimerIntervalMs),
      this,
      &SessionLengthLimiter::UpdateRemainingTime);
}

void SessionLengthLimiter::StopTimer() {
  if (!repeating_timer_)
    return;
  repeating_timer_.reset();
  UpdateRemainingTime();
}

void SessionLengthLimiter::UpdateRemainingTime() {
  const base::TimeDelta kZeroTimeDelta = base::TimeDelta();
  // If no session length limit is set, return.
  if (session_length_limit_ == kZeroTimeDelta)
    return;

  // Calculate the remaining session time, clamping so that it never falls below
  // zero.
  base::TimeDelta remaining = session_length_limit_ -
      (delegate_->GetCurrentTime() - session_start_time_);
  if (remaining < kZeroTimeDelta)
    remaining = kZeroTimeDelta;

  // End the session if the remaining time reaches zero.
  if (remaining == base::TimeDelta())
    delegate_->StopSession();
}

}  // namespace chromeos
