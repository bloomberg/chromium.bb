// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/access_token_fetcher.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "net/base/url_util.h"

namespace ntp_snippets {

using internal::SubscriptionJsonRequest;

namespace {

// Variation parameter for chrome-push-subscription backend.
const char kPushSubscriptionBackendParam[] = "push_subscription_backend";

// Variation parameter for chrome-push-unsubscription backend.
const char kPushUnsubscriptionBackendParam[] = "push_unsubscription_backend";

const char kApiKeyParamName[] = "key";
const char kAuthorizationRequestHeaderFormat[] = "Bearer %s";

}  // namespace

class SubscriptionManager::SigninObserver : public SigninManagerBase::Observer {
 public:
  SigninObserver(SigninManagerBase* signin_manager,
                 const base::Closure& signin_status_changed_callback)
      : signin_manager_(signin_manager),
        signin_status_changed_callback_(signin_status_changed_callback) {
    signin_manager_->AddObserver(this);
  }

  ~SigninObserver() override { signin_manager_->RemoveObserver(this); }

 private:
  // SigninManagerBase::Observer implementation.
  void GoogleSigninSucceeded(const std::string& account_id,
                             const std::string& username) override {
    signin_status_changed_callback_.Run();
  }

  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override {
    signin_status_changed_callback_.Run();
  }

  SigninManagerBase* const signin_manager_;
  base::Closure signin_status_changed_callback_;
};

SubscriptionManager::SubscriptionManager(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    SigninManagerBase* signin_manager,
    OAuth2TokenService* access_token_service,
    const std::string& api_key,
    const GURL& subscribe_url,
    const GURL& unsubscribe_url)
    : url_request_context_getter_(std::move(url_request_context_getter)),
      pref_service_(pref_service),
      signin_manager_(signin_manager),
      signin_observer_(base::MakeUnique<SigninObserver>(
          signin_manager,
          base::Bind(&SubscriptionManager::SigninStatusChanged,
                     base::Unretained(this)))),
      access_token_service_(access_token_service),
      api_key_(api_key),
      subscribe_url_(subscribe_url),
      unsubscribe_url_(unsubscribe_url) {}

SubscriptionManager::~SubscriptionManager() = default;

void SubscriptionManager::Subscribe(const std::string& subscription_token) {
  // If there is a request in flight, cancel it.
  if (request_) {
    request_ = nullptr;
  }
  if (signin_manager_->IsAuthenticated()) {
    StartAccessTokenRequest(subscription_token);
  } else {
    SubscribeInternal(subscription_token, /*access_token=*/std::string());
  }
}

void SubscriptionManager::SubscribeInternal(
    const std::string& subscription_token,
    const std::string& access_token) {
  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(subscription_token)
      .SetUrlRequestContextGetter(url_request_context_getter_);

  if (!access_token.empty()) {
    builder.SetUrl(subscribe_url_);
    builder.SetAuthenticationHeader(base::StringPrintf(
        kAuthorizationRequestHeaderFormat, access_token.c_str()));
  } else {
    // When not providing OAuth token, we need to pass the Google API key.
    builder.SetUrl(
        net::AppendQueryParameter(subscribe_url_, kApiKeyParamName, api_key_));
  }

  request_ = builder.Build();
  request_->Start(base::BindOnce(&SubscriptionManager::DidSubscribe,
                                 base::Unretained(this), subscription_token,
                                 /*is_authenticated=*/!access_token.empty()));
}

void SubscriptionManager::StartAccessTokenRequest(
    const std::string& subscription_token) {
  // If there is already an ongoing token request, destroy it.
  if (access_token_fetcher_) {
    access_token_fetcher_ = nullptr;
  }

  OAuth2TokenService::ScopeSet scopes = {kContentSuggestionsApiScope};
  access_token_fetcher_ = base::MakeUnique<AccessTokenFetcher>(
      "ntp_snippets", signin_manager_, access_token_service_, scopes,
      base::BindOnce(&SubscriptionManager::AccessTokenFetchFinished,
                     base::Unretained(this), subscription_token));
}

void SubscriptionManager::AccessTokenFetchFinished(
    const std::string& subscription_token,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  // Delete the fetcher only after we leave this method (which is called from
  // the fetcher itself).
  std::unique_ptr<AccessTokenFetcher> access_token_fetcher_deleter(
      std::move(access_token_fetcher_));

  if (error.state() != GoogleServiceAuthError::NONE) {
    // In case of error, we will retry on next Chrome restart.
    return;
  }
  DCHECK(!access_token.empty());
  SubscribeInternal(subscription_token, access_token);
}

void SubscriptionManager::DidSubscribe(const std::string& subscription_token,
                                       bool is_authenticated,
                                       const Status& status) {
  // Delete the request only after we leave this method (which is called from
  // the request itself).
  std::unique_ptr<internal::SubscriptionJsonRequest> request_deleter(
      std::move(request_));

  switch (status.code) {
    case StatusCode::SUCCESS:
      // In case of successful subscription, store the same data used for
      // subscription in order to be able to resubscribe in case of data
      // change.
      // TODO(mamir): Store region and language.
      pref_service_->SetString(prefs::kBreakingNewsSubscriptionDataToken,
                               subscription_token);
      pref_service_->SetBoolean(
          prefs::kBreakingNewsSubscriptionDataIsAuthenticated,
          is_authenticated);
      break;
    default:
      // TODO(mamir): Handle failure.
      break;
  }
}

void SubscriptionManager::Unsubscribe() {
  std::string token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  ResubscribeInternal(/*old_token=*/token, /*new_token=*/std::string());
}

void SubscriptionManager::ResubscribeInternal(const std::string& old_token,
                                              const std::string& new_token) {
  // If there is an request in flight, cancel it.
  if (request_) {
    request_ = nullptr;
  }

  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(old_token).SetUrlRequestContextGetter(
      url_request_context_getter_);
  builder.SetUrl(
      net::AppendQueryParameter(unsubscribe_url_, kApiKeyParamName, api_key_));

  request_ = builder.Build();
  request_->Start(base::BindOnce(&SubscriptionManager::DidUnsubscribe,
                                 base::Unretained(this), new_token));
}

bool SubscriptionManager::IsSubscribed() {
  std::string subscription_token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  return !subscription_token.empty();
}

bool SubscriptionManager::NeedsToResubscribe() {
  // Check if authentication state changed after subscription.
  bool is_auth_subscribe = pref_service_->GetBoolean(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated);
  bool is_authenticated = signin_manager_->IsAuthenticated();
  return is_auth_subscribe != is_authenticated;
}

void SubscriptionManager::Resubscribe(const std::string& new_token) {
  std::string old_token =
      pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
  if (old_token == new_token) {
    // If the token didn't change, subscribe directly. The server handles the
    // unsubscription if previous subscriptions exists.
    Subscribe(old_token);
  } else {
    ResubscribeInternal(old_token, new_token);
  }
}

void SubscriptionManager::DidUnsubscribe(const std::string& new_token,
                                         const Status& status) {
  // Delete the request only after we leave this method (which is called from
  // the request itself).
  std::unique_ptr<internal::SubscriptionJsonRequest> request_deleter(
      std::move(request_));

  switch (status.code) {
    case StatusCode::SUCCESS:
      // In case of successful unsubscription, clear the previously stored data.
      // TODO(mamir): Clear stored region and language.
      pref_service_->ClearPref(prefs::kBreakingNewsSubscriptionDataToken);
      pref_service_->ClearPref(
          prefs::kBreakingNewsSubscriptionDataIsAuthenticated);
      if (!new_token.empty()) {
        Subscribe(new_token);
      }
      break;
    default:
      // TODO(mamir): Handle failure.
      break;
  }
}

void SubscriptionManager::SigninStatusChanged() {
  // If subscribed already, resubscribe.
  if (IsSubscribed()) {
    if (request_) {
      request_ = nullptr;
    }
    std::string token =
        pref_service_->GetString(prefs::kBreakingNewsSubscriptionDataToken);
    Subscribe(token);
  }
}

void SubscriptionManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kBreakingNewsSubscriptionDataToken,
                               std::string());
  registry->RegisterBooleanPref(
      prefs::kBreakingNewsSubscriptionDataIsAuthenticated, false);
}

GURL GetPushUpdatesSubscriptionEndpoint(version_info::Channel channel) {
  std::string endpoint = base::GetFieldTrialParamValueByFeature(
      kBreakingNewsPushFeature, kPushSubscriptionBackendParam);
  if (!endpoint.empty()) {
    return GURL{endpoint};
  }

  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
      return GURL{kPushUpdatesSubscriptionServer};

    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
    case version_info::Channel::UNKNOWN:
      return GURL{kPushUpdatesSubscriptionStagingServer};
  }
  NOTREACHED();
  return GURL{kPushUpdatesSubscriptionStagingServer};
}

GURL GetPushUpdatesUnsubscriptionEndpoint(version_info::Channel channel) {
  std::string endpoint = base::GetFieldTrialParamValueByFeature(
      kBreakingNewsPushFeature, kPushUnsubscriptionBackendParam);
  if (!endpoint.empty()) {
    return GURL{endpoint};
  }

  switch (channel) {
    case version_info::Channel::STABLE:
    case version_info::Channel::BETA:
      return GURL{kPushUpdatesUnsubscriptionServer};

    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
    case version_info::Channel::UNKNOWN:
      return GURL{kPushUpdatesUnsubscriptionStagingServer};
  }
  NOTREACHED();
  return GURL{kPushUpdatesUnsubscriptionStagingServer};
}
}  // namespace ntp_snippets
