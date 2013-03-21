// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/session_length_limiter.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"

namespace chromeos {

namespace {

// The minimum session time limit that can be set.
const int kSessionLengthLimitMinMs = 30 * 1000; // 30 seconds.

// The maximum session time limit that can be set.
const int kSessionLengthLimitMaxMs = 24 * 60 * 60 * 1000; // 24 hours.

// A default delegate implementation that returns the current time and does end
// the current user's session when requested. This can be replaced with a mock
// in tests.
class SessionLengthLimiterDelegateImpl : public SessionLengthLimiter::Delegate {
 public:
  SessionLengthLimiterDelegateImpl();
  virtual ~SessionLengthLimiterDelegateImpl();

  virtual const base::TimeTicks GetCurrentTime() const OVERRIDE;
  virtual void StopSession() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionLengthLimiterDelegateImpl);
};

SessionLengthLimiterDelegateImpl::SessionLengthLimiterDelegateImpl() {
}

SessionLengthLimiterDelegateImpl::~SessionLengthLimiterDelegateImpl() {
}

const base::TimeTicks SessionLengthLimiterDelegateImpl::GetCurrentTime() const {
  return base::TimeTicks::Now();
}

void SessionLengthLimiterDelegateImpl::StopSession() {
  chrome::AttemptUserExit();
}

}  // namespace

SessionLengthLimiter::Delegate::~Delegate() {
}

// static
void SessionLengthLimiter::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kSessionStartTime, 0);
  registry->RegisterIntegerPref(prefs::kSessionLengthLimit, 0);
}

SessionLengthLimiter::SessionLengthLimiter(Delegate* delegate,
                                           bool browser_restarted)
    : delegate_(delegate ? delegate : new SessionLengthLimiterDelegateImpl) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // If this is a user login, set the session start time in local state to the
  // current time. If this a browser restart after a crash, set the session
  // start time only if its current value appears corrupted (value unset, value
  // lying in the future).
  PrefService* local_state = g_browser_process->local_state();
  int64 session_start_time = local_state->GetInt64(prefs::kSessionStartTime);
  const int64 now = delegate_->GetCurrentTime().ToInternalValue();
  if (!browser_restarted ||
      !local_state->HasPrefPath(prefs::kSessionStartTime) ||
      session_start_time > now) {
    local_state->SetInt64(prefs::kSessionStartTime, now);
    // Ensure that the session start time is persisted to local state.
    local_state->CommitPendingWrite();
    session_start_time = now;
  }
  session_start_time_ = base::TimeTicks::FromInternalValue(session_start_time);

  // Listen for changes to the session length limit.
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      prefs::kSessionLengthLimit,
      base::Bind(&SessionLengthLimiter::OnSessionLengthLimitChanged,
                 base::Unretained(this)));

  // Handle the current session length limit, if any.
  OnSessionLengthLimitChanged();
}

SessionLengthLimiter::~SessionLengthLimiter() {
}

void SessionLengthLimiter::OnSessionLengthLimitChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop any currently running timer.
  if (timer_)
    timer_->Stop();

  int limit;
  const PrefService::Preference* session_length_limit_pref =
      pref_change_registrar_.prefs()->
          FindPreference(prefs::kSessionLengthLimit);
  if (session_length_limit_pref->IsDefaultValue() ||
      !session_length_limit_pref->GetValue()->GetAsInteger(&limit)) {
    // If no session length limit is set, destroy the timer.
    timer_.reset();
    return;
  }

  // Clamp the session length limit to the valid range.
  const base::TimeDelta session_length_limit =
      base::TimeDelta::FromMilliseconds(std::min(std::max(
          limit, kSessionLengthLimitMinMs), kSessionLengthLimitMaxMs));

  // Calculate the remaining session time.
  const base::TimeDelta remaining = session_length_limit -
      (delegate_->GetCurrentTime() - session_start_time_);

  // Log out the user immediately if the session length limit has been reached
  // or exceeded.
  if (remaining <= base::TimeDelta()) {
    delegate_->StopSession();
    return;
  }

  // Set a timer to log out the user when the session length limit is reached.
  if (!timer_)
    timer_.reset(new base::OneShotTimer<SessionLengthLimiter::Delegate>);
  timer_->Start(FROM_HERE, remaining, delegate_.get(),
                &SessionLengthLimiter::Delegate::StopSession);
}

}  // namespace chromeos
