// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_id_token_decoder.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

const base::TimeDelta kRefreshAdvancedProtectionDelay =
    base::TimeDelta::FromDays(1);
const base::TimeDelta kRetryDelay = base::TimeDelta::FromMinutes(5);
const base::TimeDelta kMinimumRefreshDelay = base::TimeDelta::FromMinutes(1);
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AdvancedProtectionTokenConsumer
////////////////////////////////////////////////////////////////////////////////
AdvancedProtectionStatusManager::AdvancedProtectionTokenConsumer::
    AdvancedProtectionTokenConsumer(const std::string& account_id,
                                    AdvancedProtectionStatusManager* manager)
    : OAuth2TokenService::Consumer(account_id), manager_(manager) {}

AdvancedProtectionStatusManager::AdvancedProtectionTokenConsumer::
    ~AdvancedProtectionTokenConsumer() {}

void AdvancedProtectionStatusManager::AdvancedProtectionTokenConsumer::
    OnGetTokenSuccess(
        const OAuth2TokenService::Request* request,
        const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  manager_->OnGetIDToken(request->GetAccountId(), token_response.id_token);
  manager_->OnTokenRefreshDone(
      request, GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void AdvancedProtectionStatusManager::AdvancedProtectionTokenConsumer::
    OnGetTokenFailure(const OAuth2TokenService::Request* request,
                      const GoogleServiceAuthError& error) {
  manager_->OnTokenRefreshDone(request, error);
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedProtectionStatusManager
////////////////////////////////////////////////////////////////////////////////
AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    Profile* profile)
    : profile_(profile),
      signin_manager_(nullptr),
      account_tracker_service_(nullptr),
      is_under_advanced_protection_(false),
      minimum_delay_(kMinimumRefreshDelay) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (profile_->IsOffTheRecord())
    return;

  Initialize();
  MaybeRefreshOnStartUp();
}

void AdvancedProtectionStatusManager::Initialize() {
  signin_manager_ = SigninManagerFactory::GetForProfile(profile_);
  account_tracker_service_ =
      AccountTrackerServiceFactory::GetForProfile(profile_);
  SubscribeToSigninEvents();
}

void AdvancedProtectionStatusManager::MaybeRefreshOnStartUp() {
  // Retrieves advanced protection service status from primary account's info.
  AccountInfo info = signin_manager_->GetAuthenticatedAccountInfo();
  if (info.account_id.empty())
    return;

  is_under_advanced_protection_ = info.is_under_advanced_protection;
  primary_account_id_ = info.account_id;

  if (profile_->GetPrefs()->HasPrefPath(
          prefs::kAdvancedProtectionLastRefreshInUs)) {
    last_refreshed_ = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(profile_->GetPrefs()->GetInt64(
            prefs::kAdvancedProtectionLastRefreshInUs)));
    if (is_under_advanced_protection_)
      ScheduleNextRefresh();
  } else {
    // User's advanced protection status is unknown, refresh in
    // |minimum_delay_|.
    timer_.Start(
        FROM_HERE, minimum_delay_, this,
        &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
  }
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

bool AdvancedProtectionStatusManager::IsRefreshScheduled() {
  return timer_.IsRunning();
}

void AdvancedProtectionStatusManager::OnAccountUpdated(
    const AccountInfo& info) {
  // Ignore update if |profile_| is in incognito mode, or the updated account
  // is not the primary account.
  if (profile_->IsOffTheRecord() || !IsPrimaryAccount(info))
    return;

  if (info.is_under_advanced_protection) {
    // User just enrolled into advanced protection.
    OnAdvancedProtectionEnabled();
  } else {
    // User's no longer in advanced protection.
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::OnAccountRemoved(
    const AccountInfo& info) {
  if (profile_->IsOffTheRecord())
    return;

  // If user signed out primary account, cancel refresh.
  if (!primary_account_id_.empty() && primary_account_id_ == info.account_id) {
    primary_account_id_.clear();
    is_under_advanced_protection_ = false;
    OnAdvancedProtectionDisabled();
  }
}

void AdvancedProtectionStatusManager::GoogleSigninSucceeded(
    const AccountInfo& account_info) {
  primary_account_id_ = account_info.account_id;
  if (account_info.is_under_advanced_protection)
    OnAdvancedProtectionEnabled();
}

void AdvancedProtectionStatusManager::GoogleSignedOut(
    const AccountInfo& account_info) {
  if (primary_account_id_ == account_info.account_id)
    primary_account_id_.clear();
  OnAdvancedProtectionDisabled();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionEnabled() {
  is_under_advanced_protection_ = true;
  UpdateLastRefreshTime();
  ScheduleNextRefresh();
}

void AdvancedProtectionStatusManager::OnAdvancedProtectionDisabled() {
  is_under_advanced_protection_ = false;
  UpdateLastRefreshTime();
  CancelFutureRefresh();
}

void AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);

  if (!token_service || !IsUserSignedIn() || primary_account_id_.empty())
    return;
  // Refresh OAuth access token.
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);

  // If there's already a request going on, do nothing.
  if (access_token_request_)
    return;

  token_consumer_.reset(
      new AdvancedProtectionTokenConsumer(primary_account_id_, this));
  access_token_request_ = token_service->StartRequest(
      primary_account_id_, scopes, token_consumer_.get());
}

void AdvancedProtectionStatusManager::ScheduleNextRefresh() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CancelFutureRefresh();
  base::Time now = base::Time::Now();
  const base::TimeDelta time_since_last_refresh =
      now > last_refreshed_ ? now - last_refreshed_ : base::TimeDelta::Max();
  base::TimeDelta delay =
      time_since_last_refresh > kRefreshAdvancedProtectionDelay
          ? minimum_delay_
          : std::max(minimum_delay_,
                     kRefreshAdvancedProtectionDelay - time_since_last_refresh);
  timer_.Start(
      FROM_HERE, delay, this,
      &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
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

bool AdvancedProtectionStatusManager::IsUserSignedIn() {
  return signin_manager_ &&
         !signin_manager_->GetAuthenticatedAccountInfo().account_id.empty();
}

bool AdvancedProtectionStatusManager::IsPrimaryAccount(
    const AccountInfo& account_info) {
  return IsUserSignedIn() && account_info.account_id ==
                                 signin_manager_->GetAuthenticatedAccountId();
}

void AdvancedProtectionStatusManager::OnGetIDToken(
    const std::string& account_id,
    const std::string& id_token) {
  // Skips if the ID token is not for the primary account. Or user is no longer
  // signed in.
  if (account_id != primary_account_id_ || !IsUserSignedIn())
    return;

  gaia::TokenServiceFlags service_flags = gaia::ParseServiceFlags(id_token);
  // |OnAccountUpdated()| will only be triggered if the advanced protection
  // status changed. Therefore, we need to call |UpdateLastRefreshTime()| here
  // to force update and persist last refresh time.
  account_tracker_service_->SetIsAdvancedProtectionAccount(
      primary_account_id_, service_flags.is_under_advanced_protection);
  UpdateLastRefreshTime();
}

void AdvancedProtectionStatusManager::OnTokenRefreshDone(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK(request == access_token_request_.get());

  UMA_HISTOGRAM_ENUMERATION("SafeBrowsing.AdvancedProtection.TokenFetchStatus",
                            error.state(), GoogleServiceAuthError::NUM_STATES);
  access_token_request_.reset();

  // If failure is transient, we'll retry in 5 minutes.
  if (error.IsTransientError()) {
    timer_.Start(
        FROM_HERE, kRetryDelay, this,
        &AdvancedProtectionStatusManager::RefreshAdvancedProtectionStatus);
  }
}

AdvancedProtectionStatusManager::AdvancedProtectionStatusManager(
    Profile* profile,
    const base::TimeDelta& min_delay)
    : profile_(profile),
      signin_manager_(nullptr),
      account_tracker_service_(nullptr),
      is_under_advanced_protection_(false),
      minimum_delay_(min_delay) {
  if (profile_->IsOffTheRecord())
    return;
  Initialize();
  MaybeRefreshOnStartUp();
}

}  // namespace safe_browsing
