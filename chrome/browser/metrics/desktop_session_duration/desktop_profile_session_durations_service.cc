// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_session_duration/desktop_profile_session_durations_service.h"

#include "base/metrics/histogram_macros.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync/driver/sync_service.h"
#include "content/public/browser/browser_context.h"

namespace metrics {

DesktopProfileSessionDurationsService::DesktopProfileSessionDurationsService(
    browser_sync::ProfileSyncService* sync_service,
    GaiaCookieManagerService* cookie_manager,
    DesktopSessionDurationTracker* tracker)
    : sync_observer_(this),
      gaia_cookie_observer_(this),
      session_duration_observer_(this) {
  gaia_cookie_observer_.Add(cookie_manager);
  session_duration_observer_.Add(tracker);

  if (tracker->in_session()) {
    // The session was started before this service was created. Let's start
    // tracking now.
    OnSessionStarted(base::TimeTicks::Now());
  }

  // sync_service can be null if sync is disabled by a command line flag.
  if (sync_service != nullptr) {
    sync_observer_.Add(sync_service);
  }

  // Since this is created after the profile itself is created, we need to
  // handle the initial state.
  OnStateChanged(sync_service);

  // Check if we already know the signed in cookies. This will trigger a fetch
  // if we don't have them yet.
  std::vector<gaia::ListedAccount> signed_in;
  std::vector<gaia::ListedAccount> signed_out;
  if (cookie_manager->ListAccounts(&signed_in, &signed_out,
                                   "durations_metrics")) {
    OnGaiaAccountsInCookieUpdated(signed_in, signed_out,
                                  GoogleServiceAuthError());
  }

  DVLOG(1) << "Ready to track Session.TotalDuration metrics";
}

DesktopProfileSessionDurationsService::
    ~DesktopProfileSessionDurationsService() = default;

void DesktopProfileSessionDurationsService::OnSessionStarted(
    base::TimeTicks session_start) {
  DVLOG(1) << "Session start";
  total_session_timer_ = std::make_unique<base::ElapsedTimer>();
  signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
  sync_session_timer_ = std::make_unique<base::ElapsedTimer>();
}

namespace {
base::TimeDelta SubtractInactiveTime(base::TimeDelta total_length,
                                     base::TimeDelta inactive_time) {
  // Substract any time the user was inactive from our session length. If this
  // ends up giving the session negative length, which can happen if the feature
  // state changed after the user became inactive, log the length as 0.
  base::TimeDelta session_length = total_length - inactive_time;
  if (session_length < base::TimeDelta()) {
    session_length = base::TimeDelta();
  }
  return session_length;
}
}  // namespace

void DesktopProfileSessionDurationsService::OnSessionEnded(
    base::TimeDelta session_length) {
  DVLOG(1) << "Session end";

  if (!total_session_timer_) {
    // If there was no active session, just ignore this call.
    return;
  }

  base::TimeDelta inactivity_at_session_end =
      total_session_timer_->Elapsed() - session_length;
  LogSigninDuration(SubtractInactiveTime(signin_session_timer_->Elapsed(),
                                         inactivity_at_session_end));
  LogSyncDuration(SubtractInactiveTime(sync_session_timer_->Elapsed(),
                                       inactivity_at_session_end));
  total_session_timer_.reset();
  signin_session_timer_.reset();
  sync_session_timer_.reset();
}

void DesktopProfileSessionDurationsService::OnGaiaAccountsInCookieUpdated(
    const std::vector<gaia::ListedAccount>& accounts,
    const std::vector<gaia::ListedAccount>& signed_out_accounts,
    const GoogleServiceAuthError& error) {
  DVLOG(1) << "Cookie state change. in: " << accounts.size()
           << " out: " << signed_out_accounts.size()
           << " err: " << error.ToString();

  if (error.state() != GoogleServiceAuthError::NONE) {
    // Return early if there's an error. This should only happen if there's an
    // actual error getting the account list. If there are any auth errors with
    // the tokens, those accounts will be moved to signed_out_accounts instead.
    return;
  }

  if (accounts.empty()) {
    // No signed in account.
    if (signin_status_ == FeatureState::ON && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::OFF;
  } else {
    // There is a signed in account.
    if (signin_status_ == FeatureState::OFF && signin_session_timer_) {
      LogSigninDuration(signin_session_timer_->Elapsed());
      signin_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    signin_status_ = FeatureState::ON;
  }
}

void DesktopProfileSessionDurationsService::OnStateChanged(
    syncer::SyncService* sync) {
  DVLOG(1) << "Sync state change";
  if (sync == nullptr || !sync->CanSyncStart() ||
      sync->GetAuthError().state() ==
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    if (sync_status_ == FeatureState::ON && sync_session_timer_) {
      LogSyncDuration(sync_session_timer_->Elapsed());
      sync_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    sync_status_ = FeatureState::OFF;
  } else if (sync->IsSyncActive() &&
             sync->GetLastCycleSnapshot().is_initialized()) {
    if (sync_status_ == FeatureState::OFF && sync_session_timer_) {
      LogSyncDuration(sync_session_timer_->Elapsed());
      sync_session_timer_ = std::make_unique<base::ElapsedTimer>();
    }
    sync_status_ = FeatureState::ON;
  }
}

void DesktopProfileSessionDurationsService::LogSigninDuration(
    base::TimeDelta session_length) {
  switch (signin_status_) {
    case FeatureState::ON:
      DVLOG(1) << "Logging Session.TotalDuration.WithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithAccount",
                               session_length);
      break;
    case FeatureState::OFF:
    // Since the feature wasn't working for the user if we didn't know its
    // state, log the status as off.
    case FeatureState::UNKNOWN:
      DVLOG(1) << "Logging Session.TotalDuration.WithoutAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.WithoutAccount",
                               session_length);
  }
}

void DesktopProfileSessionDurationsService::LogSyncDuration(
    base::TimeDelta session_length) {
  // TODO(feuunk): Distinguish between being NotOptedInToSync and
  // OptedInToSyncPaused.
  switch (sync_status_) {
    case FeatureState::ON:
      DVLOG(1) << "Logging Session.TotalDuration.OptedInToSyncWithAccount of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.OptedInToSyncWithAccount",
                               session_length);
      break;
    case FeatureState::OFF:
    // Since the feature wasn't working for the user if we didn't know its
    // state, log the status as off.
    case FeatureState::UNKNOWN:
      DVLOG(1) << "Logging Session.TotalDuration.NotOptedInToSync of "
               << session_length;
      UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration.NotOptedInToSync",
                               session_length);
  }
}

void DesktopProfileSessionDurationsService::Shutdown() {
  session_duration_observer_.RemoveAll();
  gaia_cookie_observer_.RemoveAll();
  sync_observer_.RemoveAll();
}

}  // namespace metrics
