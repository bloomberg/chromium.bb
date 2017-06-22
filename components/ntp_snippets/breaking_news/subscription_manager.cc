// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/breaking_news/subscription_manager.h"
#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "components/ntp_snippets/breaking_news/subscription_json_request.h"
#include "components/ntp_snippets/features.h"
#include "components/ntp_snippets/ntp_snippets_constants.h"
#include "components/ntp_snippets/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ntp_snippets {

using internal::SubscriptionJsonRequest;

namespace {

// Variation parameter for chrome-push-subscription backend.
const char kPushSubscriptionBackendParam[] = "push_subscription_backend";

// Variation parameter for chrome-push-unsubscription backend.
const char kPushUnsubscriptionBackendParam[] = "push_unsubscription_backend";
}

SubscriptionManager::SubscriptionManager(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter,
    PrefService* pref_service,
    const GURL& subscribe_url,
    const GURL& unsubscribe_url)
    : url_request_context_getter_(std::move(url_request_context_getter)),
      pref_service_(pref_service),
      subscribe_url_(subscribe_url),
      unsubscribe_url_(unsubscribe_url) {}

SubscriptionManager::~SubscriptionManager() = default;

void SubscriptionManager::Subscribe(const std::string& token) {
  DCHECK(!subscription_request_);
  subscription_token_ = token;
  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(token)
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUrl(subscribe_url_);

  subscription_request_ = builder.Build();
  subscription_request_->Start(base::BindOnce(
      &SubscriptionManager::DidSubscribe, base::Unretained(this)));
}

bool SubscriptionManager::CanSubscribeNow() {
  if (subscription_request_) {
    return false;
  }
  return true;
}

void SubscriptionManager::DidSubscribe(const ntp_snippets::Status& status) {
  subscription_request_.reset();

  switch (status.code) {
    case ntp_snippets::StatusCode::SUCCESS:
      // In case of successful subscription, store the same data used for
      // subscription in order to be able to re-subscribe in case of data
      // change.
      // TODO(mamir): store region and language.
      pref_service_->SetString(
          ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken,
          subscription_token_);
      break;
    default:
      // TODO(mamir): handle failure.
      break;
  }
}

bool SubscriptionManager::CanUnsubscribeNow() {
  if (unsubscription_request_) {
    return false;
  }
  return true;
}

void SubscriptionManager::Unsubscribe(const std::string& token) {
  DCHECK(!unsubscription_request_);
  unsubscription_token_ = token;
  SubscriptionJsonRequest::Builder builder;
  builder.SetToken(token)
      .SetUrlRequestContextGetter(url_request_context_getter_)
      .SetUrl(unsubscribe_url_);

  unsubscription_request_ = builder.Build();
  unsubscription_request_->Start(base::BindOnce(
      &SubscriptionManager::DidUnsubscribe, base::Unretained(this)));
}

bool SubscriptionManager::IsSubscribed() {
  std::string subscription_token_ = pref_service_->GetString(
      ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken);
  return !subscription_token_.empty();
}

void SubscriptionManager::DidUnsubscribe(const ntp_snippets::Status& status) {
  unsubscription_request_.reset();

  switch (status.code) {
    case ntp_snippets::StatusCode::SUCCESS:
      // In case of successful unsubscription, clear the previously stored data.
      // TODO(mamir): clear stored region and language.
      pref_service_->ClearPref(
          ntp_snippets::prefs::kBreakingNewsSubscriptionDataToken);
      break;
    default:
      // TODO(mamir): handle failure.
      break;
  }
}

void SubscriptionManager::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kBreakingNewsSubscriptionDataToken,
                               std::string());
}

GURL GetPushUpdatesSubscriptionEndpoint(version_info::Channel channel) {
  std::string endpoint = base::GetFieldTrialParamValueByFeature(
      ntp_snippets::kBreakingNewsPushFeature, kPushSubscriptionBackendParam);
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
      ntp_snippets::kBreakingNewsPushFeature, kPushUnsubscriptionBackendParam);
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
