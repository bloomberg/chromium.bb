// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_

#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager_base.h"

class Profile;

namespace safe_browsing {

// Responsible for keeping track of advanced protection status of the sign-in
// profile.
// Note that for profile that is not signed-in or is in incognito mode, we
// consider it NOT under advanced protection.
class AdvancedProtectionStatusManager : public KeyedService,
                                        public AccountTrackerService::Observer,
                                        public SigninManagerBase::Observer {
 public:
  explicit AdvancedProtectionStatusManager(Profile* profile);
  ~AdvancedProtectionStatusManager() override;

  // If |kAdvancedProtectionStatusFeature| is enabled.
  static bool IsEnabled();

  // If the primary account of |profile| is under advanced protection.
  static bool IsUnderAdvancedProtection(Profile* profile);

  bool is_under_advanced_protection() const {
    return is_under_advanced_protection_;
  }

  // KeyedService:
  void Shutdown() override;

  // Subscribes to sign-in events.
  void SubscribeToSigninEvents();

  // Subscribes from sign-in events.
  void UnsubscribeFromSigninEvents();

 private:
  // AccountTrackerService::Observer implementations.
  void OnAccountUpdated(const AccountInfo& info) override;
  void OnAccountRemoved(const AccountInfo& info) override;

  // SigninManagerBase::Observer implementations.
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

  void OnAdvancedProtectionEnabled();

  void OnAdvancedProtectionDisabled();

  // Requests Gaia refresh token to obtain advanced protection status.
  void RefreshAdvancedProtectionStatus();

  // Requests advanced protection status and schedule next refresh.
  void RefreshAdvancedProtectionStatusAndScheduleNext();

  // Starts a timer to schedule next refresh.
  void ScheduleNextRefresh();

  // Cancels any status refresh in the future.
  void CancelFutureRefresh();

  // Sets |last_refresh_| to now and persists it.
  void UpdateLastRefreshTime();

  Profile* const profile_;

  // Is the profile account under advanced protection.
  bool is_under_advanced_protection_;

  // Gaia ID of the primary account of |profile_|. If this profile is not signed
  // in, this field will be empty.
  std::string primary_account_gaia_;

  base::OneShotTimer timer_;
  base::Time last_refreshed_;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_H_
