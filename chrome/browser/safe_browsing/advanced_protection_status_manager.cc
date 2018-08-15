// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"

namespace safe_browsing {

namespace {

const base::TimeDelta kRefreshAdvancedProtectionDelay =
    base::TimeDelta::FromDays(1);

// If |info| matches the primary account of |profile|.
bool IsPrimaryAccount(Profile* profile, const AccountInfo& info) {
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);

  return signin_manager && !info.account_id.empty() &&
         info.account_id == signin_manager->GetAuthenticatedAccountId();
}

}  // namespace

AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    Profile* profile)
    : profile_(profile), is_under_advanced_protection_(false) {
  // Retrieves advanced protection service status from primary account's info.
  if (!profile_->IsOffTheRecord()) {
    AccountInfo info = SigninManagerFactory::GetForProfileIfExists(profile)
                           ->GetAuthenticatedAccountInfo();

    // Initializes advanced protection status if user's signed-in.
    if (!info.account_id.empty()) {
      is_under_advanced_protection_ = info.is_under_advanced_protection;
      primary_account_gaia_ = info.gaia;
    }

    last_refreshed_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(profile_->GetPrefs()->GetInt64(
            prefs::kAdvancedProtectionLastRefreshInUs)));
  }
  SubscribeToSigninEvents();
}

// static
bool AdvancedProtectionStatusManager::IsEnabled() {
  return base::FeatureList::IsEnabled(kAdvancedProtectionStatusFeature);
}

void AdvancedProtectionStatusManager::Shutdown() {
  CancelFutureRefresh();
  UnsubscribeFromSigninEvents();
}

AdvancedProtectionStatusManager::~AdvancedProtectionStatusManager() {}

void AdvancedProtectionStatusManager::SubscribeToSigninEvents() {
  AccountTrackerServiceFactory::GetForProfile(profile_)->AddObserver(this);
  SigninManagerFactory::GetForProfile(profile_)->AddObserver(this);
}

void AdvancedProtectionStatusManager::UnsubscribeFromSigninEvents() {
  AccountTrackerServiceFactory::GetForProfile(profile_)->RemoveObserver(this);
  SigninManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
}

void AdvancedProtectionStatusManager::OnAccountUpdated(
    const AccountInfo& info) {
  // Ignore update if |profile_| is in incognito mode, or the updated account
  // is not the primary account.
  if (profile_->IsOffTheRecord() || !IsPrimaryAccount(profile_, info))
    return;

  if (!is_under_advanced_protection_ && info.is_under_advanced_protection) {
    // User just enrolled into advanced protection.
    OnAdvancedProtectionEnabled();
  } else if (is_under_advanced_protection_ &&
             !info.is_under_advanced_protection) {
    // User's no longer in advanced protection.
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnAccountRemoved(
    const AccountInfo& info) {
  if (profile_->IsOffTheRecord())
    return;

  // If user signed out primary account, cancel refresh.
  if (!primary_account_gaia_.empty() && primary_account_gaia_ == info.gaia) {
    primary_account_gaia_.clear();
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::GoogleSigninSucceeded(
    const AccountInfo& account_info) {
  primary_account_gaia_ = account_info.gaia;
  if (!is_under_advanced_protection_ &&
      account_info.is_under_advanced_protection) {
    OnAdvancedProtectionEnabled();
  }
}

void AdvancedProtectionStatusManager::GoogleSignedOut(
    const AccountInfo& account_info) {
  if (primary_account_gaia_ == account_info.gaia)
    primary_account_gaia_.clear();
  OnAdvancedProtectionDisabled();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionEnabled() {
  UpdateLastRefreshTime();
  is_under_advanced_protection_ = true;
  ScheduleNextRefresh();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionDisabled() {
  if (!is_under_advanced_protection_)
    return;

  is_under_advanced_protection_ = false;
  CancelFutureRefresh();
}

void AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus() {
  // To Be Implemented.
}

void AdvancedProtectionStatusManager::
    RefreshAdvancedProtectionStatusAndScheduleNext() {
  DCHECK(is_under_advanced_protection_);
  RefreshAdvancedProtectionStatus();
  UpdateLastRefreshTime();
  if (is_under_advanced_protection_)
    ScheduleNextRefresh();
}

void AdvancedProtectionStatusManager::ScheduleNextRefresh() {
  CancelFutureRefresh();
  const base::TimeDelta time_since_last_refresh =
      base::Time::Now() - last_refreshed_;
  if (time_since_last_refresh > kRefreshAdvancedProtectionDelay) {
    RefreshAdvancedProtectionStatus();
  } else {
    timer_.Start(FROM_HERE,
                 kRefreshAdvancedProtectionDelay - time_since_last_refresh,
                 this,
                 &AdvancedProtectionStatusManager::
                     RefreshAdvancedProtectionStatusAndScheduleNext);
  }
}
void AdvancedProtectionStatusManager::CancelFutureRefresh() {
  if (timer_.IsRunning())
    timer_.Stop();
}

void AdvancedProtectionStatusManager::UpdateLastRefreshTime() {
  last_refreshed_ = base::Time::Now();
  profile_->GetPrefs()->SetInt64(
      prefs::kAdvancedProtectionLastRefreshInUs,
      last_refreshed_.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

// static
bool AdvancedProtectionStatusManager::IsUnderAdvancedProtection(
    Profile* profile) {
  // Advanced protection is off for incognito mode.
  if (profile->IsOffTheRecord())
    return false;

  return AdvancedProtectionStatusManagerFactory::GetInstance()
      ->GetForBrowserContext(static_cast<content::BrowserContext*>(profile))
      ->is_under_advanced_protection();
}

}  // namespace safe_browsing
