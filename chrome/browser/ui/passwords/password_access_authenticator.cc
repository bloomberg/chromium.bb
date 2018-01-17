// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_access_authenticator.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

PasswordAccessAuthenticator::PasswordAccessAuthenticator(
    base::RepeatingCallback<bool()> os_reauth_call)
    : clock_(std::make_unique<base::DefaultClock>()),
      os_reauth_call_(std::move(os_reauth_call)) {}

PasswordAccessAuthenticator::~PasswordAccessAuthenticator() = default;

// TODO(crbug.com/327331): Trigger Re-Auth after closing and opening the
// settings tab.
bool PasswordAccessAuthenticator::EnsureUserIsAuthenticated() {
  const bool can_skip_reauth =
      last_authentication_time_.has_value() &&
      clock_->Now() - *last_authentication_time_ <=
          base::TimeDelta::FromSeconds(kAuthValidityPeriodSeconds);
  if (can_skip_reauth) {
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ReauthToAccessPasswordInSettings",
        password_manager::metrics_util::REAUTH_SKIPPED,
        password_manager::metrics_util::REAUTH_COUNT);
    return true;
  }

  return ForceUserReauthentication();
}

bool PasswordAccessAuthenticator::ForceUserReauthentication() {
  bool authenticated = os_reauth_call_.Run();
  if (authenticated)
    last_authentication_time_ = clock_->Now();
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ReauthToAccessPasswordInSettings",
      authenticated ? password_manager::metrics_util::REAUTH_SUCCESS
                    : password_manager::metrics_util::REAUTH_FAILURE,
      password_manager::metrics_util::REAUTH_COUNT);
  return authenticated;
}

void PasswordAccessAuthenticator::SetOsReauthCallForTesting(
    base::RepeatingCallback<bool()> os_reauth_call) {
  os_reauth_call_ = std::move(os_reauth_call);
}

void PasswordAccessAuthenticator::SetClockForTesting(
    std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}
