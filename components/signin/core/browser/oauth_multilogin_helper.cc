// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/oauth_multilogin_helper.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "components/signin/core/browser/oauth_multilogin_token_fetcher.h"
#include "components/signin/core/browser/signin_client.h"
#include "google_apis/gaia/oauth_multilogin_result.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr int kMaxFetcherRetries = 3;

void RecordMultiloginFinished(GoogleServiceAuthError error) {
  UMA_HISTOGRAM_ENUMERATION("Signin.MultiloginFinished", error.state(),
                            GoogleServiceAuthError::NUM_STATES);
}

std::string FindTokenForAccount(
    const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>& token_id_pairs,
    const std::string& account_id) {
  for (auto it = token_id_pairs.cbegin(); it != token_id_pairs.cend(); ++it) {
    if (account_id == it->gaia_id_)
      return it->token_;
  }
  return std::string();
}

}  // namespace

namespace signin {

OAuthMultiloginHelper::OAuthMultiloginHelper(
    SigninClient* signin_client,
    OAuth2TokenService* token_service,
    const std::vector<std::string>& account_ids,
    base::OnceCallback<void(const GoogleServiceAuthError&)> callback)
    : signin_client_(signin_client),
      token_service_(token_service),
      account_ids_(account_ids),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {
  DCHECK(signin_client_);
  DCHECK(token_service_);
  DCHECK(!account_ids_.empty());
  DCHECK(callback_);

#ifndef NDEBUG
  // Check that there is no duplicate accounts.
  std::set<std::string> accounts_no_duplicates(account_ids_.begin(),
                                               account_ids_.end());
  DCHECK_EQ(account_ids_.size(), accounts_no_duplicates.size());
#endif

  StartFetchingTokens();
}

OAuthMultiloginHelper::~OAuthMultiloginHelper() = default;

void OAuthMultiloginHelper::StartFetchingTokens() {
  DCHECK(!token_fetcher_);
  DCHECK(token_id_pairs_.empty());
  token_fetcher_ = std::make_unique<signin::OAuthMultiloginTokenFetcher>(
      signin_client_, token_service_, account_ids_,
      base::BindOnce(&OAuthMultiloginHelper::OnAccessTokensSuccess,
                     base::Unretained(this)),
      base::BindOnce(&OAuthMultiloginHelper::OnAccessTokensFailure,
                     base::Unretained(this)));
}

void OAuthMultiloginHelper::OnAccessTokensSuccess(
    const std::vector<GaiaAuthFetcher::MultiloginTokenIDPair>& token_id_pairs) {
  DCHECK(token_id_pairs_.empty());
  token_id_pairs_ = token_id_pairs;
  DCHECK_EQ(token_id_pairs_.size(), account_ids_.size());
  token_fetcher_.reset();

  signin_client_->DelayNetworkCall(
      base::BindOnce(&OAuthMultiloginHelper::StartFetchingMultiLogin,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OAuthMultiloginHelper::OnAccessTokensFailure(
    const GoogleServiceAuthError& error) {
  // Copy the error because it is owned by token_fetcher_.
  GoogleServiceAuthError error_copy = error;
  token_fetcher_.reset();
  std::move(callback_).Run(error_copy);
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartFetchingMultiLogin() {
  DCHECK_EQ(token_id_pairs_.size(), account_ids_.size());
  gaia_auth_fetcher_ = signin_client_->CreateGaiaAuthFetcher(
      this, gaia::GaiaSource::kChrome, signin_client_->GetURLLoaderFactory());
  gaia_auth_fetcher_->StartOAuthMultilogin(token_id_pairs_);
}

void OAuthMultiloginHelper::OnOAuthMultiloginFinished(
    const OAuthMultiloginResult& result) {
  RecordMultiloginFinished(result.error());

  if (result.error().state() == GoogleServiceAuthError::NONE) {
    VLOG(1) << "Multilogin successful accounts="
            << base::JoinString(account_ids_, " ");
    StartSettingCookies(result);
    return;
  }

  // If Gaia responded with status: "INVALID_TOKENS", we have to mark tokens as
  // invalid.
  if (result.error().state() ==
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
    for (const std::string& failed_account_id : result.failed_accounts()) {
      std::string failed_token =
          FindTokenForAccount(token_id_pairs_, failed_account_id);
      if (failed_token.empty()) {
        LOG(ERROR)
            << "Unexpected failed token for account not present in request: "
            << failed_account_id;
        continue;
      }
      token_service_->InvalidateTokenForMultilogin(failed_account_id,
                                                   failed_token);
    }
  }

  // Maybe some access tokens were expired, try to get new ones.
  bool is_transient_error =
      result.error().IsTransientError() || !result.failed_accounts().empty();

  if (is_transient_error && ++fetcher_retries_ < kMaxFetcherRetries) {
    UMA_HISTOGRAM_ENUMERATION("Signin.MultiloginRetry", result.error().state(),
                              GoogleServiceAuthError::NUM_STATES);
    token_id_pairs_.clear();
    StartFetchingTokens();
    return;
  }
  std::move(callback_).Run(result.error());
  // Do not add anything below this line, because this may be deleted.
}

void OAuthMultiloginHelper::StartSettingCookies(
    const OAuthMultiloginResult& result) {
  DCHECK(cookies_to_set_.empty());
  network::mojom::CookieManager* cookie_manager =
      signin_client_->GetCookieManager();
  const std::vector<net::CanonicalCookie>& cookies = result.cookies();

  for (const net::CanonicalCookie& cookie : cookies) {
    cookies_to_set_.insert(std::make_pair(cookie.Name(), cookie.Domain()));
  }
  for (const net::CanonicalCookie& cookie : cookies) {
    if (cookies_to_set_.find(std::make_pair(cookie.Name(), cookie.Domain())) !=
        cookies_to_set_.end()) {
      base::OnceCallback<void(net::CanonicalCookie::CookieInclusionStatus)>
          callback = base::BindOnce(&OAuthMultiloginHelper::OnCookieSet,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    cookie.Name(), cookie.Domain());
      net::CookieOptions options;
      options.set_include_httponly();
      // Permit it to set a SameSite cookie if it wants to.
      options.set_same_site_cookie_context(
          net::CookieOptions::SameSiteCookieContext::SAME_SITE_STRICT);
      cookie_manager->SetCanonicalCookie(
          cookie, "https", options,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              std::move(callback), net::CanonicalCookie::CookieInclusionStatus::
                                       EXCLUDE_UNKNOWN_ERROR));
    } else {
      LOG(ERROR) << "Duplicate cookie found: " << cookie.Name() << " "
                 << cookie.Domain();
    }
  }
}

void OAuthMultiloginHelper::OnCookieSet(
    const std::string& cookie_name,
    const std::string& cookie_domain,
    net::CanonicalCookie::CookieInclusionStatus status) {
  cookies_to_set_.erase(std::make_pair(cookie_name, cookie_domain));
  bool success =
      (status == net::CanonicalCookie::CookieInclusionStatus::INCLUDE);
  if (!success) {
    LOG(ERROR) << "Failed to set cookie " << cookie_name
               << " for domain=" << cookie_domain << ".";
  }
  UMA_HISTOGRAM_BOOLEAN("Signin.SetCookieSuccess", success);
  if (cookies_to_set_.empty())
    std::move(callback_).Run(GoogleServiceAuthError::AuthErrorNone());
  // Do not add anything below this line, because this may be deleted.
}

}  // namespace signin
