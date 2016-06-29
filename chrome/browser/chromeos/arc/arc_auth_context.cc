// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_context.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/arc/arc_auth_context_delegate.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

namespace arc {

namespace {

constexpr int kRefreshTokenTimeoutMs = 10 * 1000;  // 10 sec.

}  // namespace

ArcAuthContext::ArcAuthContext(ArcAuthContextDelegate* delegate,
                               Profile* profile)
    : delegate_(delegate) {
  // Reuse storage used in ARC OptIn platform app.
  const std::string site_url = base::StringPrintf(
      "%s://%s/persist?%s", content::kGuestScheme, ArcSupportHost::kHostAppId,
      ArcSupportHost::kStorageId);
  storage_partition_ = content::BrowserContext::GetStoragePartitionForSite(
      profile, GURL(site_url));
  CHECK(storage_partition_);

  // Get token service and account ID to fetch auth tokens.
  token_service_ = ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  const SigninManagerBase* const signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  CHECK(token_service_ && signin_manager);
  account_id_ = signin_manager->GetAuthenticatedAccountId();
}

ArcAuthContext::~ArcAuthContext() {
  token_service_->RemoveObserver(this);
}

void ArcAuthContext::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != account_id_)
    return;
  OnRefreshTokensLoaded();
}

void ArcAuthContext::OnRefreshTokensLoaded() {
  token_service_->RemoveObserver(this);
  refresh_token_timeout_.Stop();
  StartFetchers();
}

void ArcAuthContext::OnRefreshTokenTimeout() {
  VLOG(2) << "Failed to wait for refresh token.";
  token_service_->RemoveObserver(this);
  delegate_->OnPrepareContextFailed();
}

void ArcAuthContext::OnMergeSessionSuccess(const std::string& data) {
  context_prepared_ = true;
  delegate_->OnContextReady();
}

void ArcAuthContext::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(2) << "Failed to merge gaia session " << error.ToString() << ".";
  delegate_->OnPrepareContextFailed();
}

void ArcAuthContext::OnUbertokenSuccess(const std::string& token) {
  merger_fetcher_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeOSSource,
                          storage_partition_->GetURLRequestContext()));
  merger_fetcher_->StartMergeSession(token, std::string());
}

void ArcAuthContext::OnUbertokenFailure(const GoogleServiceAuthError& error) {
  VLOG(2) << "Failed to get ubertoken " << error.ToString() << ".";
  delegate_->OnPrepareContextFailed();
}

void ArcAuthContext::PrepareContext() {
  if (context_prepared_) {
    delegate_->OnContextReady();
    return;
  }

  token_service_->RemoveObserver(this);
  refresh_token_timeout_.Stop();

  if (!token_service_->RefreshTokenIsAvailable(account_id_)) {
    token_service_->AddObserver(this);
    refresh_token_timeout_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kRefreshTokenTimeoutMs),
        this, &ArcAuthContext::OnRefreshTokenTimeout);
    return;
  }

  StartFetchers();
}

void ArcAuthContext::StartFetchers() {
  DCHECK(!refresh_token_timeout_.IsRunning());
  merger_fetcher_.reset();
  ubertoken_fetcher_.reset(
      new UbertokenFetcher(token_service_, this, GaiaConstants::kChromeOSSource,
                           storage_partition_->GetURLRequestContext()));
  ubertoken_fetcher_->StartFetchingToken(account_id_);
}

}  // namespace arc
